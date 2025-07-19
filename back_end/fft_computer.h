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
    Left,
    Right
};

std::string channelTypeToString(ChannelType channel)
{
    switch (channel)
    {
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
            , minRenderFrequencySignal_(std::dynamic_pointer_cast<Signal<float>>(SignalManager::getInstance().getSharedSignalByName("Minimum Render Frequency")))
            , maxRenderFrequencySignal_(std::dynamic_pointer_cast<Signal<float>>(SignalManager::getInstance().getSharedSignalByName("Maximum Render Frequency")))
        {
            // Retrieve existing logger_ or create a new one
            logger_ = initializeLogger("FFT Computer", spdlog::level::info);
            inputSignalLeftChannel_ = dynamic_cast<Signal<std::vector<int32_t>>*>(SignalManager::getInstance().getSignalByName(input_signal_name_ + " Left Channel"));
            if(!inputSignalLeftChannel_)throw std::runtime_error("Failed to get signal: " + input_signal_name_ + " Left Channel");
            inputSignalRightChannel_ = dynamic_cast<Signal<std::vector<int32_t>>*>(SignalManager::getInstance().getSignalByName(input_signal_name_ + " Right Channel"));
            if(!inputSignalRightChannel_)throw std::runtime_error("Failed to get signal: " + input_signal_name_ + " Right Channel");
            fft_ = kiss_fft_alloc(fft_size_, 0, nullptr, nullptr);
            if (!fft_)
            {
                throw std::runtime_error("Failed to allocate memory for FFT.");
            }
            fftOutput_.resize(fft_size_);        
            registerCallbacks();
            fftThread_ = std::thread(&FFTComputer::processQueue, this);

            minDbSignal_ = std::dynamic_pointer_cast<Signal<float>>(SignalManager::getInstance().getSharedSignalByName("Min db"));
            if (minDbSignal_)
            {
                minDbSignal_->setValue(minDbValue_);
                minDbSignal_->registerSignalValueCallback([](const float& value, void* arg)
                {
                    FFTComputer* self = static_cast<FFTComputer*>(arg);
                    self->minDbValue_ = value;
                    self->logger_->info("FFT Computer: Received new Min Db value: {}", value);
                }, this);
            }
            else
            {
                logger_->warn("FFT Computer: Min db signal not found, using default value: {}", minDbValue_);
            }

            maxDbSignal_ = std::dynamic_pointer_cast<Signal<float>>(SignalManager::getInstance().getSharedSignalByName("Max db"));
            if (maxDbSignal_)
            {
                maxDbSignal_->setValue(maxDbValue_);
                maxDbSignal_->registerSignalValueCallback([](const float& value, void* arg)
                {
                    FFTComputer* self = static_cast<FFTComputer*>(arg);
                    self->maxDbValue_ = value;
                    self->logger_->info("FFT Computer: Received new Max Db value: {}", value);
                }, this);
            }
            else
            {
                logger_->warn("FFT Computer: Max db signal not found, using default value: {}", maxDbValue_);
            }
            
            auto minRenderFrequencySignal = minRenderFrequencySignal_.lock();
            if (minRenderFrequencySignal)
            {
                logger_->info("Minimum Render Frequency signal initialized successfully.");
                minRenderFrequencySignal->registerSignalValueCallback([this](const float& value, void* arg) {
                    std::lock_guard<std::mutex> lock(this->mutex_);
                    this->logger_->info("Minimum Render Frequency Signal Callback: {}", value);
                    this->minRenderFrequency_ = value;
                }, this);
            }
            else
            {
                logger_->warn("Minimum Render Frequency Signal not found, using default value: 0.0f");
                minRenderFrequency_ = 0.0f; // Default initialization
            }

            auto maxRenderFrequencySignal = maxRenderFrequencySignal_.lock();
            if (maxRenderFrequencySignal)
            {
                logger_->info("Maximum Render Frequency signal initialized successfully.");
                maxRenderFrequencySignal->registerSignalValueCallback([this](const float& value, void* arg) {
                    std::lock_guard<std::mutex> lock(this->mutex_);
                    this->logger_->info("Maximum Render Frequency Signal Callback: {}", value);
                    this->maxRenderFrequency_ = value;
                }, this);
            }
            else
            {
                logger_->warn("Maximum Render Frequency Signal not found, using default value: 24000.0f");
                maxRenderFrequency_ = 24000.0f; // Default initialization
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
                std::lock_guard<std::mutex> lock(mutex_);
                dataQueue_.emplace(data, channel);
            }
            cv_.notify_one();
        }

        void registerFFTCallback(const std::function<void(const std::vector<float>&, ChannelType)>& callback)
        {
            fftCallback_ = callback;
        }

        std::array<float, 32> GetFFTBandCenters() const
        {
            return ISO_32_BAND_CENTERS;
        };

    private:
        struct DataPacket
        {
            std::vector<int32_t> data;
            ChannelType channel;
            DataPacket() : data(), channel(ChannelType::Left) {}
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
        std::mutex mutex_;
        std::condition_variable cv_;
        std::queue<DataPacket> dataQueue_;
        kiss_fft_cfg fft_;
        std::vector<kiss_fft_cpx> fftOutput_;
        std::function<void(const std::vector<float>&, ChannelType)> fftCallback_;
        const float sqrt2 = std::sqrt(2.0);
        std::shared_ptr<spdlog::logger> logger_;

        Signal<std::vector<int32_t>>* inputSignalLeftChannel_;
        Signal<std::vector<int32_t>>* inputSignalRightChannel_;
        std::shared_ptr<Signal<std::vector<float>>> leftChannelOutputSignal_ = SignalManager::getInstance().createSignal<std::vector<float>>(output_signal_name_ + " Left Channel", webSocketServer_, get_fft_bands_encoder(GetIsoBandLabels()));
        std::shared_ptr<Signal<std::vector<float>>> rightChannelOutputSignal_ = SignalManager::getInstance().createSignal<std::vector<float>>(output_signal_name_ + " Right Channel", webSocketServer_, get_fft_bands_encoder(GetIsoBandLabels()));
        std::shared_ptr<Signal<std::string>> leftLoudnessChannelSignal_ = SignalManager::getInstance().createSignal<std::string>(output_signal_name_ + " Left Channel Loudness", webSocketServer_, get_signal_and_value_encoder<std::string>());
        std::shared_ptr<Signal<std::string>> rightLoudnessChannelSignal_ = SignalManager::getInstance().createSignal<std::string>(output_signal_name_ + " Right Channel Loudness", webSocketServer_, get_signal_and_value_encoder<std::string>());
        std::shared_ptr<Signal<BinData>> leftBinDataSignal_ = SignalManager::getInstance().createSignal<BinData>(output_signal_name_ + " Left Bin Data", webSocketServer_, get_bin_data_encoder());
        std::shared_ptr<Signal<BinData>> rightBinDataSignal_ = SignalManager::getInstance().createSignal<BinData>(output_signal_name_ + " Right Bin Data", webSocketServer_, get_bin_data_encoder());

        std::shared_ptr<Signal<float>> minDbSignal_;
        float minDbValue_ = 0.0f;
        std::shared_ptr<Signal<float>> maxDbSignal_;
        float maxDbValue_ = 40.0f;

        float minRenderFrequency_ = 0.0f;
        std::weak_ptr<Signal<float>> minRenderFrequencySignal_;

        float maxRenderFrequency_ = 24000.0f;
        std::weak_ptr<Signal<float>> maxRenderFrequencySignal_;

        static std::vector<std::string> GetIsoBandLabels()
        {
            std::vector<std::string> labels;
            labels.reserve(ISO_32_BAND_CENTERS.size());

            std::transform(
                ISO_32_BAND_CENTERS.begin(),
                ISO_32_BAND_CENTERS.end(),
                std::back_inserter(labels),
                [](float f)
                {
                    std::ostringstream oss;
                    oss.precision(1);
                    oss << std::fixed << f;
                    return oss.str();
                }
            );

            return labels;
        }

        void registerCallbacks()
        {
            auto callback = [](const std::vector<int32_t>& value, void* arg, ChannelType channel)
            {
                FFTComputer* self = static_cast<FFTComputer*>(arg);
                spdlog::get("FFT Computer")->debug("Device {}: Received {} channel values:", self->name_, channelTypeToString(channel));
                self->addData(value, channel);
            };

            inputSignalLeftChannel_->registerSignalValueCallback( [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Left); }, this );
            inputSignalRightChannel_->registerSignalValueCallback( [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Right); }, this );
        }

        void unregisterCallbacks()
        {
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
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait(lock, [this] { return !dataQueue_.empty() || stopFlag_; });
                    if (stopFlag_) break;
                    dataPacket = std::move(dataQueue_.front());
                    dataQueue_.pop();
                }

                // Select buffer based on channel
                std::vector<int32_t>* buffer = nullptr;
                switch (dataPacket.channel)
                {
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
            float loudnessDb = 0.0f;
            std::vector<kiss_fft_cpx> inputData(fft_size_);
            std::vector<float> inputDataNormalized(fft_size_, 0.0f);
            int count = std::min(fft_size_, dataPacket.data.size());
            for (size_t i = 0; i < count; ++i)
            {
                inputData[i].r = static_cast<float>(dataPacket.data[i]);
                inputData[i].i = 0;
                inputDataNormalized[i] = inputData[i].r / maxValue_;
                loudnessDb += inputDataNormalized[i] * inputDataNormalized[i];
            }

            if (count > 0)
            {
                loudnessDb /= count; // mean square
                loudnessDb = std::sqrt(loudnessDb); // RMS
                loudnessDb = 20.0f * std::log10(loudnessDb + 1e-6f); // dBFS, +epsilon to avoid log(0)
                
                // Convert to approximate dB SPL using INMP441 sensitivity
                loudnessDb += 120.0f;
            }
            else
            {
                loudnessDb = -120.0f; // or appropriate silence floor
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
                case ChannelType::Left:
                    logger_->debug("Device {}: Set Left Output Signal Value:", name_);
                    leftChannelOutputSignal_->setValue(saeBands);
                    leftBinDataSignal_->setValue(binData);
                    leftLoudnessChannelSignal_->setValue(std::to_string(loudnessDb));
                break;
                case ChannelType::Right:
                    logger_->debug("Device {}: Set Right Output Signal Value:", name_);
                    rightChannelOutputSignal_->setValue(saeBands);
                    rightBinDataSignal_->setValue(binData);
                    rightLoudnessChannelSignal_->setValue(std::to_string(loudnessDb));
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

        float normalizeDb(float amplitude)
        {
            float db = 20.0f * std::log10(amplitude + 1e-6f);
            float normalized = (db - minDbValue_) / (maxDbValue_ - minDbValue_);
            return std::clamp(normalized, 0.0f, 1.0f);
        }

        void computeSAEBands(const std::vector<float>& magnitudes, std::vector<float>& saeBands, BinData& binData)
        {
            float freqResolution = static_cast<float>(sampleRate_) / fft_size_;

            // Convert min/max frequency to bin range
            size_t minAllowedBin = static_cast<size_t>(std::floor(minRenderFrequency_ / freqResolution));
            size_t maxAllowedBin = static_cast<size_t>(std::ceil(maxRenderFrequency_ / freqResolution));
            minAllowedBin = std::min(minAllowedBin, fft_size_ - 1);
            maxAllowedBin = std::min(maxAllowedBin, fft_size_ - 1);

            // Initialize binData
            binData.normalizedMinValue = std::numeric_limits<float>::max();
            binData.normalizedMaxValue = std::numeric_limits<float>::lowest();
            binData.minBin = minAllowedBin;
            binData.maxBin = maxAllowedBin;

            // Find min/max amplitude within allowed bin range
            for (size_t i = minAllowedBin; i <= maxAllowedBin; ++i)
            {
                if (magnitudes[i] < binData.normalizedMinValue)
                {
                    binData.normalizedMinValue = magnitudes[i];
                    binData.minBin = static_cast<uint16_t>(i);
                }
                if (magnitudes[i] > binData.normalizedMaxValue)
                {
                    binData.normalizedMaxValue = magnitudes[i];
                    binData.maxBin = static_cast<uint16_t>(i);
                }
            }

            // Normalize to 0–1.0
            binData.normalizedMinValue = normalizeDb(binData.normalizedMinValue);
            binData.normalizedMaxValue = normalizeDb(binData.normalizedMaxValue);

            // Compute all 32 SAE bands
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
                    // RMS
                    saeBands[i] = std::sqrt(saeBands[i] / static_cast<float>(count));
                    saeBands[i] = normalizeDb(saeBands[i]);
                }
                else
                {
                    saeBands[i] = 0.0f;
                }
            }

            // totalBins now reflects only the bins within the min/max render frequency
            binData.totalBins = static_cast<uint16_t>(maxAllowedBin - minAllowedBin + 1);
        }

        static constexpr std::array<float, 32> ISO_32_BAND_CENTERS =
        {
            16.0, 20.0, 25.0, 31.5, 40.0, 50.0, 63.0, 80.0, 100.0, 125.0, 160.0, 200.0, 250.0, 315.0, 400.0, 500.0, 630.0,
            800.0, 1000.0, 1250.0, 1600.0, 2000.0, 2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0, 10000.0, 12500.0, 16000.0, 20000.0
        };

        double getFFTFrequency(int binIndex)
        {
            return (sampleRate_ / fft_size_) * binIndex;
        }
};
