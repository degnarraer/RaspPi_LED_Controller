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
#include "signals/IntVectorSignal.h"
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
            // Retrieve existing logger_ or create a new one
            logger_ = initializeLogger("FFT Computer", spdlog::level::info);
            inputSignal_ = dynamic_cast<Signal<std::vector<int32_t>>*>(SignalManager::getInstance().getSignalByName(input_signal_name_));
            if(!inputSignal_)throw std::runtime_error("Failed to get signal: " + input_signal_name_);
            inputSignalLeftChannel_ = dynamic_cast<Signal<std::vector<int32_t>>*>(SignalManager::getInstance().getSignalByName(input_signal_name_ + " Left Channel"));
            if(!inputSignal_)throw std::runtime_error("Failed to get signal: " + input_signal_name_ + " Left Channel");
            inputSignalRightChannel_ = dynamic_cast<Signal<std::vector<int32_t>>*>(SignalManager::getInstance().getSignalByName(input_signal_name_ + " Right Channel"));
            if(!inputSignal_)throw std::runtime_error("Failed to get signal: " + input_signal_name_ + " Right Channel");
            fft_ = kiss_fft_alloc(fft_size_, 0, nullptr, nullptr);
            if (!fft_)
            {
                throw std::runtime_error("Failed to allocate memory for FFT.");
            }
            fftOutput_.resize(fft_size_);        
            registerCallbacks();
            fftThread_ = std::thread(&FFTComputer::processQueue, this);

            minDbSignalCallback_ = [](const float& value, void* arg)
            {
                FFTComputer* self = static_cast<FFTComputer*>(arg);
                self->minDbValue_ = value;
                self->logger_->debug("FFT Computer: Received new Min Db value: {}", value);
            };

            if (minDbSignal_)
            {
                minDbSignal_->setValue(0);
                minDbSignal_->registerSignalValueCallback(minDbSignalCallback_, this);
            }

            maxDbSignalCallback_ = [](const float& value, void* arg)
            {
                FFTComputer* self = static_cast<FFTComputer*>(arg);
                self->maxDbValue_ = value;
                self->logger_->debug("FFT Computer: Received new Max Db value: {}", value);
            };

            if (maxDbSignal_)
            {
                maxDbSignal_->setValue(85);
                maxDbSignal_->registerSignalValueCallback(maxDbSignalCallback_, this);
            }

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
        std::shared_ptr<spdlog::logger> logger_;

        Signal<std::vector<int32_t>>* inputSignal_;
        Signal<std::vector<int32_t>>* inputSignalLeftChannel_;
        Signal<std::vector<int32_t>>* inputSignalRightChannel_;
        std::shared_ptr<Signal<std::vector<float>>> monoOutputSignal_ = SignalManager::getInstance().createSignal<std::vector<float>>(output_signal_name_, webSocketServer_, get_fft_bands_encoder());
        std::shared_ptr<Signal<std::vector<float>>> leftChannelOutputSignal_ = SignalManager::getInstance().createSignal<std::vector<float>>(output_signal_name_ + " Left Channel", webSocketServer_, get_fft_bands_encoder());
        std::shared_ptr<Signal<std::vector<float>>> rightChannelOutputSignal_ = SignalManager::getInstance().createSignal<std::vector<float>>(output_signal_name_ + " Right Channel", webSocketServer_, get_fft_bands_encoder());
        std::shared_ptr<Signal<BinData>> monoBinDataSignal_ = SignalManager::getInstance().createSignal<BinData>(output_signal_name_ + " Mono Bin Data", webSocketServer_, get_bin_data_encoder());
        std::shared_ptr<Signal<BinData>> leftBinDataSignal_ = SignalManager::getInstance().createSignal<BinData>(output_signal_name_ + " Left Bin Data", webSocketServer_, get_bin_data_encoder());
        std::shared_ptr<Signal<BinData>> rightBinDataSignal_ = SignalManager::getInstance().createSignal<BinData>(output_signal_name_ + " Right Bin Data", webSocketServer_, get_bin_data_encoder());
        
        std::shared_ptr<Signal<float>> minDbSignal_;
        float minDbValue_ = 0.0f;
        std::shared_ptr<Signal<float>> maxDbSignal_;
        float maxDbValue_ = 85.0f;
        std::function<void(const float&, void*)> minDbSignalCallback_;
        std::function<void(const float&, void*)> maxDbSignalCallback_;

        void registerCallbacks()
        {
            auto callback = [](const std::vector<int32_t>& value, void* arg, ChannelType channel)
            {
                FFTComputer* self = static_cast<FFTComputer*>(arg);
                spdlog::get("FFT Computer")->debug("Device {}: Received {} channel values:", self->name_, channelTypeToString(channel));
                self->addData(value, channel);
            };

            inputSignal_->registerSignalValueCallback( [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Mono); }, this );
            inputSignalLeftChannel_->registerSignalValueCallback( [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Left); }, this );
            inputSignalRightChannel_->registerSignalValueCallback( [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Right); }, this );
        }

        void unregisterCallbacks()
        {
            inputSignal_->unregisterSignalValueCallbackByArg(this);
            inputSignalLeftChannel_->unregisterSignalValueCallbackByArg(this);
            inputSignalRightChannel_->unregisterSignalValueCallbackByArg(this);
        }

        void processQueue()
        {
            std::vector<int32_t> monoBuffer;
            std::vector<int32_t> leftBuffer;
            std::vector<int32_t> rightBuffer;
            const size_t minimumStepSize = 512;
            const size_t requiredSamples = fft_size_;
            const size_t overlapSamples = std::min(fft_size_ - 512, std::max(minimumStepSize, fft_size_));
            const size_t nonOverlappingSamples = requiredSamples - overlapSamples;

            while (!stopFlag_)
            {
                DataPacket dataPacket;
                {
                    std::unique_lock<std::mutex> lock(queueMutex_);
                    cv_.wait(lock, [this] { return !dataQueue_.empty() || stopFlag_; });
                    if (stopFlag_) break;
                    dataPacket = std::move(dataQueue_.front());
                    dataQueue_.pop();
                }

                // Select buffer based on channel
                std::vector<int32_t>* buffer = nullptr;
                switch (dataPacket.channel)
                {
                    case ChannelType::Mono: buffer = &monoBuffer; break;
                    case ChannelType::Left: buffer = &leftBuffer; break;
                    case ChannelType::Right: buffer = &rightBuffer; break;
                    default: continue;
                }

                buffer->insert(buffer->end(), std::make_move_iterator(dataPacket.data.begin()), std::make_move_iterator(dataPacket.data.end()));

                while (buffer->size() >= requiredSamples)
                {
                    std::vector<int32_t> fftData(buffer->begin(), buffer->begin() + requiredSamples);
                    buffer->erase(buffer->begin(), buffer->begin() + nonOverlappingSamples);
                    processFFT({ std::move(fftData), dataPacket.channel });
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
            BinData binData;
            computeSAEBands(magnitudes, saeBands, binData);
            logSAEBands(saeBands);

            if (fftCallback_)
            {
                fftCallback_(saeBands, dataPacket.channel);
            }
            switch(dataPacket.channel)
            {
                case ChannelType::Mono:
                    logger_->debug("Device {}: Set Mono Output Signal Value:", name_);
                    monoOutputSignal_->setValue(saeBands);
                    monoBinDataSignal_->setValue(binData);
                break;
                case ChannelType::Left:
                    logger_->debug("Device {}: Set Left Output Signal Value:", name_);
                    leftChannelOutputSignal_->setValue(saeBands);
                    leftBinDataSignal_->setValue(binData);
                break;
                case ChannelType::Right:
                    logger_->debug("Device {}: Set Right Output Signal Value:", name_);
                    rightChannelOutputSignal_->setValue(saeBands);
                    rightBinDataSignal_->setValue(binData);
                break;
                default:
                    logger_->error("Device {}: Unsupported channel type:", name_);
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
            logger_->trace("SAE Band Values: {}", result);
        }

        void computeSAEBands(const std::vector<float>& magnitudes, std::vector<float>& saeBands, BinData& binData)
        {
            float freqResolution = static_cast<float>(sampleRate_) / fft_size_;

            // Initialize min/max amplitude and bin indices
            binData.minValue = std::numeric_limits<float>::max();
            binData.maxValue = std::numeric_limits<float>::lowest();
            binData.minBin = 0;
            binData.maxBin = 0;

            // Find min/max amplitude and their bin indices across all magnitudes
            for (size_t i = 0; i < magnitudes.size(); ++i)
            {
                if (magnitudes[i] < binData.minValue)
                {
                    binData.minValue = magnitudes[i];
                    binData.minBin = static_cast<uint16_t>(i);
                }
                if (magnitudes[i] > binData.maxValue)
                {
                    binData.maxValue = magnitudes[i];
                    binData.maxBin = static_cast<uint16_t>(i);
                }
            }

            binData.minValue = 20.0f * std::log10(binData.minValue + 1e-6f);
            binData.maxValue = 20.0f * std::log10(binData.maxValue + 1e-6f);

            const float min_dB = -80.0f; // Threshold for silence
            const float max_dB = 60.0f;   // Maximum expected dB

            // Compute SAE bands
            for (size_t i = 0; i < ISO_32_BAND_CENTERS.size(); ++i)
            {
                float lowerFreq, upperFreq;

                if (i == 0)
                {
                    lowerFreq = ISO_32_BAND_CENTERS[i] / sqrt2;
                }
                else
                {
                    lowerFreq = (ISO_32_BAND_CENTERS[i - 1] + ISO_32_BAND_CENTERS[i]) / 2.0f;
                }

                if (i == ISO_32_BAND_CENTERS.size() - 1)
                {
                    upperFreq = ISO_32_BAND_CENTERS[i] * sqrt2;
                }
                else
                {
                    upperFreq = (ISO_32_BAND_CENTERS[i] + ISO_32_BAND_CENTERS[i + 1]) / 2.0f;
                }

                size_t binStart = static_cast<size_t>(std::floor(lowerFreq / freqResolution));
                size_t binEnd = static_cast<size_t>(std::ceil(upperFreq / freqResolution));

                binStart = std::min(binStart, fft_size_ - 1);
                binEnd = std::min(binEnd, fft_size_ - 1);

                saeBands[i] = 0.0f;
                size_t count = 0;

                for (size_t j = binStart; j <= binEnd; ++j)
                {
                    saeBands[i] += magnitudes[j] * magnitudes[j]; // power
                    ++count;
                }

                if (count > 0)
                {
                    saeBands[i] = std::sqrt(saeBands[i] / static_cast<float>(count)); // RMS

                    // Convert to dB
                    saeBands[i] = 20.0f * std::log10(saeBands[i] + 1e-6f); // avoid log(0)

                    // Normalize to 0–1.0
                    saeBands[i] = (saeBands[i] - min_dB) / (max_dB - min_dB);
                    saeBands[i] = std::clamp(saeBands[i], 0.0f, 1.0f);
                }
                else
                {
                    saeBands[i] = 0.0f;
                }
            }

            binData.totalBins = static_cast<uint16_t>(fft_size_ / 2);
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
