#pragma once
#include <vector>
#include <iostream>
#include <stdexcept>
#include <alsa/asoundlib.h>
#include <thread>
#include <functional>
#include <atomic>
#include <chrono>
#include "logger.h"
#include "signals/IntVectorSignal.h"
#include "websocket_server.h"

class I2SMicrophone 
{
    public:
        I2SMicrophone( const std::string& targetDevice
                     , const std::string& signal_Name
                     , unsigned int sampleRate
                     , unsigned int channels
                     , unsigned int numFrames
                     , _snd_pcm_format snd_pcm_format
                     , _snd_pcm_access snd_pcm_access
                     , bool allowResampling
                     , unsigned int latency
                     , std::shared_ptr<WebSocketServer> webSocketServer );
        ~I2SMicrophone();
        std::vector<int32_t> ReadAudioData();
        void StartReadingMicrophone();
        void StartReadingSineWave(double frequency);
        void StopReading();
        void SplitAudioData(const std::vector<int32_t>& buffer);
        std::string find_device(std::string targetDevice);

        
        std::string targetDevice_;
        std::shared_ptr<spdlog::logger> logger_;

    private:
        std::string signal_Name_;
        unsigned int sampleRate_;
        unsigned int channels_;
        unsigned int numFrames_;
        std::shared_ptr<WebSocketServer> webSocketServer_;
        snd_pcm_t* handle_ = nullptr;
        std::atomic<bool> stopReading_;
        std::thread readingThread_;
        std::thread sineWaveThread_;
        std::shared_ptr<Signal<std::vector<int32_t>>> inputSignal_;
        std::shared_ptr<Signal<std::vector<int32_t>>> inputSignalLeftChannel_;
        std::shared_ptr<Signal<std::vector<int32_t>>> inputSignalRightChannel_;
        std::shared_ptr<Signal<std::string>> minMicrophoneSignal_;
        std::shared_ptr<Signal<std::string>> maxMicrophoneSignal_;
        std::function<void(const std::vector<int32_t>&, void*)> microphoneSignalCallback_;
        std::function<void(const std::vector<int32_t>&, void*)> microphoneLeftChannelSignalCallback_;
        std::function<void(const std::vector<int32_t>&, void*)> microphoneRightChannelSignalCallback_;
        std::function<void(const std::string&, void*)> minMicrophoneSignalCallback_;
        std::function<void(const std::string&, void*)> maxMicrophoneSignalCallback_;
};
