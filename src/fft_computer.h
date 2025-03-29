#include "kiss_fft.h"
#include <vector>
#include <iostream>
#include <spdlog/spdlog.h>
#include "ring_buffer.h"
#include "signal.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <array>
#include <cmath>

enum class ChannelType
{
    Mono,
    Left,
    Right
};

std::string channelTypeToString(ChannelType channel)
{
    switch (channel)
    {
        case ChannelType::Mono:
            return "Mono";
        case ChannelType::Left:
            return "Left";
        case ChannelType::Right:
            return "Right";
        default:
            return "Unknown";  // In case a new value is added or if an invalid value is passed
    }
}

static constexpr std::array<float, 32> SAE_BAND_CENTERS =
{
    20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630,
    800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000
};

class FFTComputer
{
    public:
        FFTComputer(const std::string name, const std::string input_signal_name, size_t fft_bin_size, unsigned int sampleRate, int32_t maxValue)
            : name_(name), input_signal_name_(input_signal_name), fft_bin_size_(fft_bin_size), sampleRate_(sampleRate), maxValue_(maxValue), stopFlag_(false)
        {
            // Retrieve existing logger or create a new one
            logger = spdlog::get("FFT Computer Logger");
            if (!logger)
            {
                logger = spdlog::stdout_color_mt("FFT Computer Logger");
                spdlog::register_logger(logger);
            }

            fft_ = kiss_fft_alloc(fft_bin_size_, 0, nullptr, nullptr);
            if (!fft_)
            {
                throw std::runtime_error("Failed to allocate memory for FFT.");
            }
            fftOutput_.resize(fft_bin_size_);        
            registerCallbacks();
            fftThread_ = std::thread(&FFTComputer::processQueue, this);
        }

        ~FFTComputer()
        {
            stopFlag_ = true;
            cv_.notify_all();
            if (fftThread_.joinable())
            {
                fftThread_.join();
            }

            unregisterCallbacks();
            if (fft_)
            {
                free(fft_);
            }
        }

        void addData(const std::vector<int32_t>& data, ChannelType channel)
        {
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                dataQueue_.emplace(data, channel);
            }
            cv_.notify_one();
        }

        void registerFFTCallback(const std::function<void(const std::vector<float>&, ChannelType)>& callback)
        {
            fftCallback_ = callback;
        }

    private:
        struct DataPacket
        {
            std::vector<int32_t> data;
            ChannelType channel;
            DataPacket() : data(), channel(ChannelType::Mono) {}
            DataPacket(std::vector<int32_t> d, ChannelType c) : data(std::move(d)), channel(c) {}
        };

        std::string name_;
        std::string input_signal_name_;
        std::string output_signal_name_;
        size_t fft_bin_size_;
        unsigned int sampleRate_;
        int32_t maxValue_;

        std::atomic<bool> stopFlag_;
        std::thread fftThread_;
        std::mutex queueMutex_;
        std::condition_variable cv_;
        std::queue<DataPacket> dataQueue_;
        kiss_fft_cfg fft_;
        std::vector<kiss_fft_cpx> fftOutput_;
        std::function<void(const std::vector<float>&, ChannelType)> fftCallback_;
        std::shared_ptr<spdlog::logger> logger;

        void registerCallbacks()
        {
            auto callback = [](const std::vector<int32_t>& value, void* arg, ChannelType channel)
            {
                FFTComputer* self = static_cast<FFTComputer*>(arg);
                spdlog::get("FFT Computer Logger")->debug("Device {}: Received {} channel values:", self->name_, channelTypeToString(channel));
                self->addData(value, channel);
            };

            SignalManager::GetInstance().GetSignal<std::vector<int32_t>>(input_signal_name_)->RegisterCallback(
                [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Mono); }, this);
            SignalManager::GetInstance().GetSignal<std::vector<int32_t>>(input_signal_name_ + "_Left_Channel")->RegisterCallback(
                [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Left); }, this);
            SignalManager::GetInstance().GetSignal<std::vector<int32_t>>(input_signal_name_ + "_Right_Channel")->RegisterCallback(
                [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Right); }, this);
        }

        void unregisterCallbacks()
        {
            SignalManager::GetInstance().GetSignal<std::vector<int32_t>>(input_signal_name_)->UnregisterCallbackByArg(this);
            SignalManager::GetInstance().GetSignal<std::vector<int32_t>>(input_signal_name_ + "_Left_Channel")->UnregisterCallbackByArg(this);
            SignalManager::GetInstance().GetSignal<std::vector<int32_t>>(input_signal_name_ + "_Right_Channel")->UnregisterCallbackByArg(this);
        }

        void processQueue()
        {
            while (!stopFlag_)
            {
                DataPacket dataPacket{};
                {
                    std::unique_lock<std::mutex> lock(queueMutex_);
                    cv_.wait(lock, [this] { return !dataQueue_.empty() || stopFlag_; });
                    if (stopFlag_) break;
                    dataPacket = std::move(dataQueue_.front());
                    dataQueue_.pop();
                }
                processFFT(dataPacket);
            }
        }

        void processFFT(const DataPacket& dataPacket)
        {
            std::vector<kiss_fft_cpx> inputData(fft_bin_size_);
            for (size_t i = 0; i < std::min(fft_bin_size_, dataPacket.data.size()); ++i)
            {
                inputData[i].r = static_cast<float>(dataPacket.data[i]);
                inputData[i].i = 0;
            }

            kiss_fft(fft_, inputData.data(), fftOutput_.data());

            std::vector<float> magnitudes(fft_bin_size_, 0.0f);
            std::vector<float> saeBands(32, 0.0f);
            const float max_int32 = std::numeric_limits<int32_t>::max();
            for (size_t i = 0; i < fft_bin_size_; ++i)
            {
                magnitudes[i] = sqrt(fftOutput_[i].r * fftOutput_[i].r + fftOutput_[i].i * fftOutput_[i].i);
                magnitudes[i] /= maxValue_;
            }

            computeSAEBands(magnitudes, saeBands);
            logSAEBands(saeBands);

            if (fftCallback_)
            {
                fftCallback_(saeBands, dataPacket.channel);
            }
            switch(dataPacket.channel)
            {
                case ChannelType::Mono:
                    logger->debug("Device {}: Set Mono Output Signal Value:", name_);
                    SignalManager::GetInstance().GetSignal<std::vector<float>>(output_signal_name_)->SetValue(saeBands);
                break;
                case ChannelType::Left:
                    logger->debug("Device {}: Set Left Output Signal Value:", name_);
                    SignalManager::GetInstance().GetSignal<std::vector<float>>(output_signal_name_ + "_Left_Channel")->SetValue(saeBands);
                break;
                case ChannelType::Right:
                    logger->debug("Device {}: Set Right Output Signal Value:", name_);
                    SignalManager::GetInstance().GetSignal<std::vector<float>>(output_signal_name_ + "_Right_Channel")->SetValue(saeBands);
                break;
                default:
                    logger->error("Device {}: Unsupported channel type:", name_);
                break;
            }
        }

        void logSAEBands(std::vector<float>& saeBands) const
        {
            std::string result;
            for (size_t i = 0; i < saeBands.size(); ++i)
            {
                if(i > 0) result += " ";
                result += fmt::format("{:.1f}", saeBands[i]);
            }
            logger->trace("SAE Band Values: {}", result);
        }

        void computeSAEBands(const std::vector<float>& magnitudes, std::vector<float>& saeBands)
        {
            float freqResolution = static_cast<float>(sampleRate_) / fft_bin_size_;
            for (size_t i = 0; i < 32; ++i)
            {
                float bandCenter = SAE_BAND_CENTERS[i];
                size_t binStart = static_cast<size_t>((bandCenter / std::sqrt(2.0)) / freqResolution);
                size_t binEnd = static_cast<size_t>((bandCenter * std::sqrt(2.0)) / freqResolution);
                
                for (size_t j = binStart; j < binEnd && j < fft_bin_size_; ++j)
                {
                    saeBands[i] += magnitudes[j];
                }
                saeBands[i] /= (binEnd - binStart + 1);
            }
        }
};
