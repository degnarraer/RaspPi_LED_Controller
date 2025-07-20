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

class FFTComputer
{
    public:
        FFTComputer( const std::string name
                   , const std::string input_signal_name
                   , const std::string output_signal_name
                   , size_t fft_size
                   , unsigned int sampleRate
                   , int32_t maxValue
                   , std::shared_ptr<WebSocketServer> webSocketServer);

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
                kiss_fft_free(fft_);
            }
        }
        
        enum class ChannelType
        {
            Left,
            Right
        };

        void addData(const std::vector<int32_t>& data, ChannelType channel);
        void registerFFTCallback(const std::function<void(const std::vector<float>&, ChannelType)>& callback);
        std::array<float, 32> GetFFTBandCenters() const;
        std::string channelTypeToString(ChannelType channel);

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

        //Signals for input
        Signal<std::vector<int32_t>>* inputSignalLeftChannel_;
        Signal<std::vector<int32_t>>* inputSignalRightChannel_;


        //Signals for output
        SignalManager& signalManager_ = SignalManager::getInstance();
        std::shared_ptr<Signal<std::vector<float>>> leftChannelFFTOutputSPLSignal_ = signalManager_.createSignal<std::vector<float>>(output_signal_name_ + " Left Channel FFT SPL", webSocketServer_, get_fft_bands_encoder(GetIsoBandLabels()));
        std::shared_ptr<Signal<std::vector<float>>> rightChannelFFTOutputSPLSignal_ = signalManager_.createSignal<std::vector<float>>(output_signal_name_ + " Right Channel FFT SPL", webSocketServer_, get_fft_bands_encoder(GetIsoBandLabels()));
        std::shared_ptr<Signal<std::vector<float>>> leftChannelFFTOutputNormalizedSignal_ = signalManager_.createSignal<std::vector<float>>(output_signal_name_ + " Left Channel FFT Normalized", webSocketServer_, get_fft_bands_encoder(GetIsoBandLabels()));
        std::shared_ptr<Signal<std::vector<float>>> rightChannelFFTOutputNormalizedSignal_ = signalManager_.createSignal<std::vector<float>>(output_signal_name_ + " Right Channel FFT Normalized", webSocketServer_, get_fft_bands_encoder(GetIsoBandLabels()));
        std::shared_ptr<Signal<std::string>> leftChannelPowerSPLSignal_ = signalManager_.createSignal<std::string>(output_signal_name_ + " Left Channel Power SPL", webSocketServer_, get_signal_and_value_encoder<std::string>());
        std::shared_ptr<Signal<std::string>> rightChannelPowerSPLSignal_ = signalManager_.createSignal<std::string>(output_signal_name_ + " Right Channel Power SPL", webSocketServer_, get_signal_and_value_encoder<std::string>());
        std::shared_ptr<Signal<std::string>> leftChannelPowerNormalizedSignal_ = signalManager_.createSignal<std::string>(output_signal_name_ + " Left Channel Power Normalized", webSocketServer_, get_signal_and_value_encoder<std::string>());
        std::shared_ptr<Signal<std::string>> rightChannelPowerNormalizedSignal_ = signalManager_.createSignal<std::string>(output_signal_name_ + " Right Channel Power Normalized", webSocketServer_, get_signal_and_value_encoder<std::string>());
        std::shared_ptr<Signal<BinData>> leftBinDataSignal_ = signalManager_.createSignal<BinData>(output_signal_name_ + " Left Bin Data", webSocketServer_, get_bin_data_encoder());
        std::shared_ptr<Signal<BinData>> rightBinDataSignal_ = signalManager_.createSignal<BinData>(output_signal_name_ + " Right Bin Data", webSocketServer_, get_bin_data_encoder());

        static constexpr float micOffsetDb = 120.0f;
        std::shared_ptr<Signal<float>> minDbSignal_;
        float minDbValue_ = 30.0f; // Whisper
        std::shared_ptr<Signal<float>> maxDbSignal_;
        float maxDbValue_ = 90.0f; // City Traffic

        float minRenderFrequency_ = 0.0f;
        std::weak_ptr<Signal<float>> minRenderFrequencySignal_;

        float maxRenderFrequency_ = sampleRate_ / 2.0f;
        std::weak_ptr<Signal<float>> maxRenderFrequencySignal_;

        static std::vector<std::string> GetIsoBandLabels();
        void registerCallbacks();
        void unregisterCallbacks();
        void processQueue();
        void processFFT(const DataPacket& dataPacket);
        void logSAEBands(std::vector<float>& saeBands) const;
        float normalizeDb(float amplitude);
        void computeFFTBinData(const std::vector<float>& magnitudes, BinData& binData);
        void computeSAEBands(const std::vector<float>& magnitudes, std::vector<float>& saeBands);

        static constexpr std::array<float, 32> ISO_32_BAND_CENTERS =
        {
            16.0, 20.0, 25.0, 31.5, 40.0, 50.0, 63.0, 80.0, 100.0, 125.0, 160.0, 200.0, 250.0, 315.0, 400.0, 500.0, 630.0,
            800.0, 1000.0, 1250.0, 1600.0, 2000.0, 2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0, 10000.0, 12500.0, 16000.0, 20000.0
        };

        double getFFTFrequency(int binIndex);
};
