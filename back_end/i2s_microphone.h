#pragma once

#include <vector>
#include <iostream>
#include <stdexcept>
#include <alsa/asoundlib.h>
#include <thread>
#include <functional>
#include <atomic>
#include <chrono>
#include <memory>

#include "logger.h"
#include "signals/IntVectorSignal.h"
#include "websocket_server.h"
#include "guarddog.h"

// ALSA handle RAII wrapper
struct SndPcmHandleDeleter
{
    void operator()(snd_pcm_t* handle) const
    {
        if (handle)
        {
            snd_pcm_close(handle);
        }
    }
};

using SndPcmHandlePtr = std::unique_ptr<snd_pcm_t, SndPcmHandleDeleter>;

class I2SMicrophone
{
public:
    I2SMicrophone(const std::string& targetDevice,
                  const std::string& signal_Name,
                  unsigned int sampleRate,
                  unsigned int numFrames,
                  _snd_pcm_format snd_pcm_format,
                  _snd_pcm_access snd_pcm_access,
                  bool allowResampling,
                  unsigned int latency,
                  std::shared_ptr<WebSocketServer> webSocketServer);

    ~I2SMicrophone();

    std::vector<int32_t> readAudioData();
    void startReadingMicrophone();
    void startReadingSineWave(double frequency);
    void stopReading();
    void splitAudioData(const std::vector<int32_t>& buffer);
    std::string find_device(const std::string& targetDevice);

    std::string targetDevice_;
    std::shared_ptr<spdlog::logger> logger_;

private:
    std::string signal_Name_;
    unsigned int sampleRate_;
    unsigned int numFrames_;
    std::shared_ptr<WebSocketServer> webSocketServer_;
    std::shared_ptr<GuardDog> guarddog_;
    SndPcmHandlePtr handle_;
    std::atomic<bool> stopReading_{false};
    std::thread readingThread_;
    std::thread sineWaveThread_;
    SignalManager& signalManager_ = SignalManager::getInstance();
    std::shared_ptr<Signal<std::vector<int32_t>>> inputSignalLeftChannel_;
    std::shared_ptr<Signal<std::vector<int32_t>>> inputSignalRightChannel_;
    std::shared_ptr<Signal<float>> minDbSignal_;
    std::shared_ptr<Signal<float>> maxDbSignal_;
};
