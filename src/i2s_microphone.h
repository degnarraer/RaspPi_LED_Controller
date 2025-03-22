#pragma once
#include <vector>
#include <iostream>
#include <stdexcept>
#include <alsa/asoundlib.h>
#include <thread>
#include <functional>
#include <atomic>
#include <chrono>
#include <spdlog/spdlog.h>
#include "signal.h"

class I2SMicrophone {
public:
    I2SMicrophone(const std::string& deviceName, unsigned int sampleRate, unsigned int channels, unsigned int numFrames, _snd_pcm_format snd_pcm_format, _snd_pcm_access snd_pcm_access)
        : deviceName_(deviceName), sampleRate_(sampleRate), channels_(channels), numFrames_(numFrames), stopReading_(false){
        list_devices();
        if (snd_pcm_open(&handle_, deviceName.c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0) {
            throw std::runtime_error("Failed to open I2S microphone: " + std::string(snd_strerror(errno)));
        } else {
            spdlog::get("Microphone Logger")->info("Device {}: Opening device", deviceName_);
            if (snd_pcm_set_params(handle_,
                                snd_pcm_format,
                                snd_pcm_access,
                                channels_,
                                sampleRate_,
                                1,                  // Allow resampling
                                200000 ) < 0) {     // 0.5s latency
                throw std::runtime_error("Failed to set ALSA parameters: " + std::string(snd_strerror(errno)));
            } else {
                spdlog::get("Microphone Logger")->info("Device {}: Opened", deviceName_);
            }
        }

        microphoneSignal = new Signal<std::vector<int32_t>>("Microphone");
        if(microphoneSignal == nullptr)
        {
            throw std::runtime_error("Failed to set create Microphone Signal for: " + deviceName_);
        }
        microphoneSignalCallback_ = [](const std::vector<int32_t>& value, void* arg) {
            I2SMicrophone* self = static_cast<I2SMicrophone*>(arg);
            spdlog::get("Microphone Logger")->debug("Device {}: Received new values:", self->deviceName_);
            for (int32_t v : value) {
                spdlog::get("Microphone Logger")->trace("Device {}: Value:{}", self->deviceName_, v);
            }
        };
        microphoneSignal->RegisterCallback(microphoneSignalCallback_, this);


        
        microphoneLeftChannelSignal = new Signal<std::vector<int32_t>>("Microphone_Left_Channel");
        if(microphoneLeftChannelSignal == nullptr)
        {
            throw std::runtime_error("Failed to set create Microphone Left Channel Signal for: " + deviceName_);
        }
        microphoneLeftChannelSignalCallback_ = [](const std::vector<int32_t>& value, void* arg) {
            I2SMicrophone* self = static_cast<I2SMicrophone*>(arg);
            spdlog::get("Microphone Logger")->debug("Device {}: Received new Left Channel values:", self->deviceName_);
            for (int32_t v : value) {
                spdlog::get("Microphone Logger")->trace("Device {}: Value:{}", self->deviceName_, v);
            }
        };
        microphoneLeftChannelSignal->RegisterCallback(microphoneLeftChannelSignalCallback_, this);

        microphoneRightChannelSignal = new Signal<std::vector<int32_t>>("Microphone_Right_Channel");
        if(microphoneRightChannelSignal == nullptr)
        {
            throw std::runtime_error("Failed to set create Microphone Right Channel Signal for: " + deviceName_);
        }
        microphoneRightChannelSignalCallback_ = [](const std::vector<int32_t>& value, void* arg) {
            I2SMicrophone* self = static_cast<I2SMicrophone*>(arg);
            spdlog::get("Microphone Logger")->debug("Device {}: Received new Right Channel values:", self->deviceName_);
            for (int32_t v : value) {
                spdlog::get("Microphone Logger")->trace("Device {}: Value:{}", self->deviceName_, v);
            }
        };
        microphoneRightChannelSignal->RegisterCallback(microphoneRightChannelSignalCallback_, this);

    }

    ~I2SMicrophone() {
        stopReading_ = true;
        microphoneSignal->UnregisterCallbackByArg(this);
        delete microphoneSignal;
        if (readingThread_.joinable()) {
            readingThread_.join();
        }
        if (handle_) {
            snd_pcm_close(handle_);
        }
    }

    std::vector<int32_t> ReadAudioData() {
        spdlog::get("Microphone Logger")->debug("Device {}: ReadAudioData: Start", deviceName_);
        std::vector<int32_t> buffer(numFrames_ * channels_);
        int framesRead = snd_pcm_readi(handle_, buffer.data(), numFrames_);
        if (framesRead < 0) {
            spdlog::get("Microphone Logger")->error("Device {}: ReadAudioData: Error reading audio data: {}", deviceName_, snd_strerror(framesRead));
            if (snd_pcm_recover(handle_, framesRead, 1) < 0) {
                spdlog::get("Microphone Logger")->error("Device {}: ReadAudioData: Failed to recover from error: {} Resetting Stream.", deviceName_, snd_strerror(framesRead));
                snd_pcm_prepare(handle_);  
            } else {
                spdlog::get("Microphone Logger")->debug("Device {}: ReadAudioData: Recovered from error", deviceName_);
            }
        } else if (framesRead != static_cast<int>(numFrames_)) {
            spdlog::get("Microphone Logger")->warn("Device {}: ReadAudioData: Partial read ({} frames read, expected {})", deviceName_, framesRead, numFrames_);
        } else {
            spdlog::get("Microphone Logger")->debug("Device {}: ReadAudioData: Complete", deviceName_);
        }
        return buffer;
    }

    // Start reading audio data in a separate thread and call all registered callbacks when buffer is full
    void StartReading() {
        spdlog::get("Microphone Logger")->debug("Device {}: StartReading", deviceName_);
        readingThread_ = std::thread([this]() {
            while (!stopReading_) {
                std::vector<int32_t> buffer = ReadAudioData();
                if (!buffer.empty()) {
                    switch(channels_){
                        case 1:
                            microphoneSignal->SetValue(buffer);
                        break;
                        case 2:
                            SplitAudioData(buffer);
                        break;
                        default:
                            spdlog::get("Microphone Logger")->error("Device {}: Invalid channel config.", deviceName_);
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Small delay to prevent high CPU usage
            }
        });
    }

    void SplitAudioData(const std::vector<int32_t>& buffer) {
        if (channels_ != 2) {
            return;
        }
        spdlog::get("Microphone Logger")->debug("Device {}: Audio data split started", deviceName_);
        // Reserve space for left and right channels
        std::vector<int32_t> leftChannel(numFrames_);
        std::vector<int32_t> rightChannel(numFrames_);

        // Split interleaved stereo data into separate left and right channels
        for (size_t i = 0; i < numFrames_; ++i) {
        // Extract the left and right samples (assuming buffer is interleaved)
        leftChannel.push_back(buffer[i * channels_]);         // Even indices are left channel
        rightChannel.push_back(buffer[i * channels_ + 1]);    // Odd indices are right channel
        }
        microphoneLeftChannelSignal->SetValue(leftChannel);
        microphoneRightChannelSignal->SetValue(rightChannel);
        spdlog::get("Microphone Logger")->debug("Device {}: Audio data split complete", deviceName_);
    }

    void list_devices() {
        snd_ctl_t *handle;
        snd_ctl_card_info_t *info;
        int card = -1;
        int err;

        snd_ctl_card_info_alloca(&info);

        // Iterate over all sound cards
        while (snd_card_next(&card) >= 0 && card >= 0) {
            std::string card_name = "hw:" + std::to_string(card);

            // Open control interface for the card
            if ((err = snd_ctl_open(&handle, card_name.c_str(), 0)) < 0) {
                spdlog::get("Microphone Logger")->error("Device {}: Control open error for card {} : {}", deviceName_, card_name, snd_strerror(err));
                continue;
            }

            // Get card info
            if ((err = snd_ctl_card_info(handle, info)) < 0) {
                spdlog::get("Microphone Logger")->error("Device {}: Control card info error for card {} : {}", deviceName_, card_name, snd_strerror(err));
                snd_ctl_close(handle);
                continue;
            }

            spdlog::get("Microphone Logger")->info("Device {}: Card: {}", deviceName_, snd_ctl_card_info_get_name(info));
            spdlog::get("Microphone Logger")->info("Device {}: Driver: {}", deviceName_, snd_ctl_card_info_get_driver(info));

            // List all PCM devices (both playback and capture)
            int pcm_device = -1;
            while (snd_ctl_pcm_next_device(handle, &pcm_device) >= 0 && pcm_device >= 0) {
                spdlog::get("Microphone Logger")->info("Device {}: PCM device: {}", deviceName_, pcm_device);
            }

            snd_ctl_close(handle);
        }
    }
public:
    // i2s Microphone Device Name
    std::string deviceName_;
private:
    unsigned int sampleRate_;
    unsigned int channels_;
    unsigned int numFrames_;
    snd_pcm_t* handle_ = nullptr;
    std::atomic<bool> stopReading_;  // Flag to stop reading when destructor is called
    std::thread readingThread_;  // Thread to read audio data asynchronously
    Signal<std::vector<int32_t>> *microphoneSignal = nullptr;
    std::function<void(const std::vector<int32_t>&, void*)> microphoneSignalCallback_;

    Signal<std::vector<int32_t>> *microphoneLeftChannelSignal = nullptr;
    std::function<void(const std::vector<int32_t>&, void*)> microphoneLeftChannelSignalCallback_;
    
    Signal<std::vector<int32_t>> *microphoneRightChannelSignal = nullptr;
    std::function<void(const std::vector<int32_t>&, void*)> microphoneRightChannelSignalCallback_;
};
