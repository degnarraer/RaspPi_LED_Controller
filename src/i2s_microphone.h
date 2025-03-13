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


class I2SMicrophone {
public:
    I2SMicrophone(const std::string& deviceName, unsigned int sampleRate, unsigned int channels, unsigned int numFrames)
        : deviceName_(deviceName), sampleRate_(sampleRate), channels_(channels), numFrames_(numFrames), stopReading_(false) {
            list_devices();
            if (snd_pcm_open(&handle_, deviceName.c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0) {
                throw std::runtime_error("Failed to open I2S microphone: " + std::string(snd_strerror(errno)));
            } else {

            }
            spdlog::get("Microphone Logger")->info("Opening device: {}", deviceName_);
            if (snd_pcm_set_params(handle_,
                                SND_PCM_FORMAT_S32_LE,
                                SND_PCM_ACCESS_RW_INTERLEAVED,
                                channels_,
                                sampleRate_,
                                1,               // Allow resampling
                                500000) < 0) {   // 0.5s latency
                throw std::runtime_error("Failed to set ALSA parameters: " + std::string(snd_strerror(errno)));
            } else {
                spdlog::get("Microphone Logger")->info("{} Opened", deviceName_);
            }
    }

    ~I2SMicrophone() {
        stopReading_ = true;
        if (readingThread_.joinable()) {
            readingThread_.join();
        }
        if (handle_) {
            snd_pcm_close(handle_);
        }
    }

    std::vector<int32_t> ReadAudioData() {
        spdlog::get("Microphone Logger")->debug("{} ReadAudioData: Started", deviceName_);
        std::vector<int32_t> buffer(numFrames_ * channels_);
        int framesRead = snd_pcm_readi(handle_, buffer.data(), numFrames_);
        if (framesRead < 0) {
            spdlog::get("Microphone Logger")->error("Error reading audio data: {}", snd_strerror(framesRead));
            snd_pcm_recover(handle_, framesRead, 1);
        } else if (framesRead != static_cast<int>(numFrames_)) {
            spdlog::get("Microphone Logger")->warn("Warning: Partial read ({} frames read, expected {})", framesRead, numFrames_);
        } else {
            spdlog::get("Microphone Logger")->debug("{} ReadAudioData: Completed", deviceName_);
        }
        return buffer;
    }

    // Start reading audio data in a separate thread and call the callback when buffer is full
    void StartReading(const std::function<void(const std::vector<int32_t>&, const std::string&)>& callback) {
        spdlog::get("Microphone Logger")->debug("{} StartReading", deviceName_);
        callback_ = callback;
        readingThread_ = std::thread([this]() {
            while (!stopReading_) {
                std::vector<int32_t> buffer = ReadAudioData();
                if (!buffer.empty()) {
                    spdlog::get("Microphone Logger")->trace("{} Read Data.", deviceName_);
                    callback_(buffer, deviceName_);
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
                spdlog::get("Microphone Logger")->error("Control open error for card {} : {}", card_name, snd_strerror(err));
                continue;
            }
    
            // Get card info
            if ((err = snd_ctl_card_info(handle, info)) < 0) {
                spdlog::get("Microphone Logger")->error("Control card info error for card {} : {}", card_name, snd_strerror(err));
                snd_ctl_close(handle);
                continue;
            }
    
            spdlog::get("Microphone Logger")->info("Card: {}", snd_ctl_card_info_get_name(info));
            spdlog::get("Microphone Logger")->info("Driver: {}", snd_ctl_card_info_get_driver(info));
    
            // List all PCM devices (both playback and capture)
            int pcm_device = -1;
            while (snd_ctl_pcm_next_device(handle, &pcm_device) >= 0 && pcm_device >= 0) {
                spdlog::get("Microphone Logger")->info("PCM device: {}", pcm_device);
            }
    
            snd_ctl_close(handle);
        }
    }
private:
    std::string deviceName_;
    unsigned int sampleRate_;
    unsigned int channels_;
    unsigned int numFrames_;
    snd_pcm_t* handle_ = nullptr;
    std::function<void(const std::vector<int32_t>&, const std::string&)> callback_; // Store the callback function
    std::atomic<bool> stopReading_; // Flag to stop reading when destructor is called
    std::thread readingThread_; // Thread to read audio data asynchronously
};
