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
    I2SMicrophone(const std::string& deviceName, unsigned int sampleRate, unsigned int channels, unsigned int numFrames)
        : deviceName_(deviceName), sampleRate_(sampleRate), channels_(channels), numFrames_(numFrames), stopReading_(false){
        list_devices();
        if (snd_pcm_open(&handle_, deviceName.c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0) {
            throw std::runtime_error("Failed to open I2S microphone: " + std::string(snd_strerror(errno)));
        } else {
            spdlog::get("Microphone Logger")->info("Device {}: Opening device", deviceName_);
            if (snd_pcm_set_params(handle_,
                                SND_PCM_FORMAT_S32_LE,
                                SND_PCM_ACCESS_RW_INTERLEAVED,
                                channels_,
                                sampleRate_,
                                1,               // Allow resampling
                                500000) < 0) {   // 0.5s latency
                throw std::runtime_error("Failed to set ALSA parameters: " + std::string(snd_strerror(errno)));
            } else {
                spdlog::get("Microphone Logger")->info("Device {}: Opened", deviceName_);
            }
        }

        microphoneSignalCallback_ = [](const std::vector<int32_t>& value, void* arg) {
            I2SMicrophone* self = static_cast<I2SMicrophone*>(arg);
            spdlog::get("Microphone Logger")->debug("Device {}: Received new values:", self->deviceName_);
            for (int32_t v : value) {
                spdlog::get("Microphone Logger")->trace("Device {}: Value:{}", self->deviceName_, v);
            }
        };

        microphoneSignal.RegisterCallback(microphoneSignalCallback_, this);
    }

    ~I2SMicrophone() {
        stopReading_ = true;
        microphoneSignal.UnregisterCallbackByArg(this);
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
                spdlog::get("Microphone Logger")->error("Device {}: ReadAudioData: Failed to recover from error: {}", deviceName_, snd_strerror(framesRead));
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

    // Register a callback function
    void RegisterCallback(const std::function<void(const std::vector<int32_t>&, const std::string&)>& callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        spdlog::get("Microphone Logger")->debug("Device {}: Callback Registered", deviceName_);
        callbacks_.push_back(callback);
    }

    // Deregister a callback function
    void DeregisterCallback(const std::function<void(const std::vector<int32_t>&, const std::string&)>& callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);

        // Find and remove the callback by comparing the target function pointers
        auto it = std::find_if(callbacks_.begin(), callbacks_.end(), 
            [&](const std::function<void(const std::vector<int32_t>&, const std::string&)>& cb) {
                return cb.target<void(*)(const std::vector<int32_t>&, const std::string&)>() == callback.target<void(*)(const std::vector<int32_t>&, const std::string&)>();
            });

        if (it != callbacks_.end()) {
            callbacks_.erase(it);
            spdlog::get("Microphone Logger")->debug("Device {}: Callback removed!", deviceName_);
        } else {
            spdlog::get("Microphone Logger")->warn("Device {}: Callback not found!", deviceName_);
        }
    }

    // Start reading audio data in a separate thread and call all registered callbacks when buffer is full
    void StartReading() {
        spdlog::get("Microphone Logger")->debug("Device {}: StartReading", deviceName_);
        readingThread_ = std::thread([this]() {
            while (!stopReading_) {
                std::vector<int32_t> buffer = ReadAudioData();
                if (!buffer.empty()) {
                    spdlog::get("Microphone Logger")->trace("Device {}: Read Data.", deviceName_);
                    // Call all registered callbacks
                    {
                        std::lock_guard<std::mutex> lock(callbackMutex_);
                        for (const auto& callback : callbacks_) {
                            callback(buffer, deviceName_);
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Small delay to prevent high CPU usage
            }
        });
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
    std::string deviceName_;
private:
    unsigned int sampleRate_;
    unsigned int channels_;
    unsigned int numFrames_;
    snd_pcm_t* handle_ = nullptr;
    std::vector<std::function<void(const std::vector<int32_t>&, const std::string&)>> callbacks_;  // Vector of callback functions
    std::atomic<bool> stopReading_;  // Flag to stop reading when destructor is called
    std::thread readingThread_;  // Thread to read audio data asynchronously
    std::mutex callbackMutex_;  // Mutex to synchronize callback registration/deregistration
    Signal<std::vector<int32_t>> microphoneSignal = Signal<std::vector<int32_t>>("Microphone");
    std::function<void(const std::vector<int32_t>&, void*)> microphoneSignalCallback_;
};
