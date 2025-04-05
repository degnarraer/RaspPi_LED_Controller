#pragma once
#include <vector>
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <array>
#include <cmath>
#include "logger.h"
#include "kiss_fft.h"
#include "ring_buffer.h"
#include "signal.h"
#include "websocket_server.h"

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

class FFTComputer
{
    public:
        FFTComputer( const std::string name
                   , const std::string input_signal_name
                   , const std::string output_signal_name
                   , size_t fft_size
                   , unsigned int sampleRate
                   , int32_t maxValue
                   , std::shared_ptr<WebSocketServer> webSocketServer)
            : name_(name)
            , input_signal_name_(input_signal_name)
            , output_signal_name_(output_signal_name)
            , fft_size_(fft_size)
            , sampleRate_(sampleRate)
            , maxValue_(maxValue)
            , webSocketServer_(webSocketServer)
            , stopFlag_(false)
        {
            // Retrieve existing logger or create a new one
            logger = InitializeLogger("FFT Computer", spdlog::level::info);

            fft_ = kiss_fft_alloc(fft_size_, 0, nullptr, nullptr);
            if (!fft_)
            {
                throw std::runtime_error("Failed to allocate memory for FFT.");
            }
            fftOutput_.resize(fft_size_);        
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
        size_t fft_size_;
        unsigned int sampleRate_;
        int32_t maxValue_;
        std::shared_ptr<WebSocketServer> webSocketServer_;

        std::atomic<bool> stopFlag_;
        std::thread fftThread_;
        std::mutex queueMutex_;
        std::condition_variable cv_;
        std::queue<DataPacket> dataQueue_;
        kiss_fft_cfg fft_;
        std::vector<kiss_fft_cpx> fftOutput_;
        std::function<void(const std::vector<float>&, ChannelType)> fftCallback_;
        const float sqrt2 = std::sqrt(2.0);
        std::shared_ptr<spdlog::logger> logger;

        void registerCallbacks()
        {
            auto callback = [](const std::vector<int32_t>& value, void* arg, ChannelType channel)
            {
                FFTComputer* self = static_cast<FFTComputer*>(arg);
                spdlog::get("FFT Computer")->debug("Device {}: Received {} channel values:", self->name_, channelTypeToString(channel));
                self->addData(value, channel);
            };

            SignalManager::GetInstance().GetSignal<std::vector<int32_t>>(input_signal_name_)->RegisterCallback(
                [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Mono); }, this);
            SignalManager::GetInstance().GetSignal<std::vector<int32_t>>(input_signal_name_ + " Left Channel")->RegisterCallback(
                [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Left); }, this);
            SignalManager::GetInstance().GetSignal<std::vector<int32_t>>(input_signal_name_ + " Right Channel")->RegisterCallback(
                [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Right); }, this);
        }

        void unregisterCallbacks()
        {
            SignalManager::GetInstance().GetSignal<std::vector<int32_t>>(input_signal_name_)->UnregisterCallbackByArg(this);
            SignalManager::GetInstance().GetSignal<std::vector<int32_t>>(input_signal_name_ + " Left Channel")->UnregisterCallbackByArg(this);
            SignalManager::GetInstance().GetSignal<std::vector<int32_t>>(input_signal_name_ + " Right Channel")->UnregisterCallbackByArg(this);
        }

        void processQueue()
        {
            std::vector<int32_t> accumulatedData;  // Buffer for incoming data
            size_t requiredSamples = fft_size_;    // 8192 samples required for FFT
            size_t overlapSamples = fft_size_ / 4; // 75% overlap (2048 samples)
            size_t nonOverlappingSamples = requiredSamples - overlapSamples; // Non-overlapping part

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

                // Accumulate the incoming data
                accumulatedData.insert(accumulatedData.end(), dataPacket.data.begin(), dataPacket.data.end());

                // Process when we have enough data for the FFT with overlap
                while (accumulatedData.size() >= requiredSamples)
                {
                    // Take the first `requiredSamples` from the accumulated data for FFT
                    std::vector<int32_t> fftData(accumulatedData.begin(), accumulatedData.begin() + requiredSamples);

                    // Remove the processed data from the front of the buffer, but keep the overlap
                    accumulatedData.erase(accumulatedData.begin(), accumulatedData.begin() + nonOverlappingSamples);

                    // Process FFT on this chunk of data
                    DataPacket fftPacket{ std::move(fftData), dataPacket.channel };
                    processFFT(fftPacket);  // Call the existing FFT processing function
                }
            }
        }

        void processFFT(const DataPacket& dataPacket)
        {
            std::vector<kiss_fft_cpx> inputData(fft_size_);
            for (size_t i = 0; i < std::min(fft_size_, dataPacket.data.size()); ++i)
            {
                inputData[i].r = static_cast<float>(dataPacket.data[i]);
                inputData[i].i = 0;
            }

            kiss_fft(fft_, inputData.data(), fftOutput_.data());

            std::vector<float> magnitudes(fft_size_, 0.0f);
            std::vector<float> saeBands(32, 0.0f);
            const float max_int32 = std::numeric_limits<int32_t>::max();
            for (size_t i = 0; i < fft_size_; ++i)
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
                    SignalManager::GetInstance().GetSignal<std::vector<float>>(output_signal_name_, webSocketServer_, encode_FFT_Bands)->SetValue(saeBands);
                break;
                case ChannelType::Left:
                    logger->debug("Device {}: Set Left Output Signal Value:", name_);
                    SignalManager::GetInstance().GetSignal<std::vector<float>>(output_signal_name_ + " Left Channel", webSocketServer_, encode_FFT_Bands)->SetValue(saeBands);
                break;
                case ChannelType::Right:
                    logger->debug("Device {}: Set Right Output Signal Value:", name_);
                    SignalManager::GetInstance().GetSignal<std::vector<float>>(output_signal_name_ + " Right Channel", webSocketServer_, encode_FFT_Bands)->SetValue(saeBands);
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
            float freqResolution = static_cast<float>(sampleRate_) / fft_size_;

            for (size_t i = 0; i < ISO_32_BAND_CENTERS.size(); ++i)
            {
                float lowerFreq, upperFreq;

                if (i == 0)
                {
                    lowerFreq = ISO_32_BAND_CENTERS[i] / sqrt2; // First band: Lower bound is half of center
                }
                else
                {
                    lowerFreq = (ISO_32_BAND_CENTERS[i - 1] + ISO_32_BAND_CENTERS[i]) / 2.0f;
                }

                if (i == ISO_32_BAND_CENTERS.size() - 1)
                {
                    upperFreq = ISO_32_BAND_CENTERS[i] * sqrt2; // Last band: Upper bound extends beyond
                }
                else
                {
                    upperFreq = (ISO_32_BAND_CENTERS[i] + ISO_32_BAND_CENTERS[i + 1]) / 2.0f;
                }

                size_t binStart = static_cast<size_t>(std::floor(lowerFreq / freqResolution));
                size_t binEnd = static_cast<size_t>(std::ceil(upperFreq / freqResolution));

                // Ensure bin ranges are within bounds
                binStart = std::min(binStart, fft_size_ - 1);
                binEnd = std::min(binEnd, fft_size_ - 1);

                saeBands[i] = 0.0f;
                size_t count = 0;

                for (size_t j = binStart; j <= binEnd; ++j)
                {
                    saeBands[i] += magnitudes[j] * magnitudes[j]; // Sum squared magnitudes (power)
                    ++count;
                }

                // Compute power-based normalization (RMS of the power in the band)
                if (count > 0)
                {
                    saeBands[i] = std::sqrt(saeBands[i] / static_cast<float>(count)); // Square root of mean power
                }
            }
        }
 
        static constexpr std::array<float, 32> ISO_32_BAND_CENTERS =
        {
            16, 20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630,
            800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000
        };

        double getFFTFrequency(int binIndex)
        {
            return (sampleRate_ / fft_size_) * binIndex;
        }
};
