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
            if (snd_pcm_open(&handle_, deviceName.c_str(), SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK) < 0) {
                throw std::runtime_error("Failed to open I2S microphone: " + std::string(snd_strerror(errno)));
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

    // Start reading audio data in a separate thread and call the callback when buffer is full
    void StartReading(const std::function<void(const std::vector<int32_t>&)>& callback) {
        callback_ = callback;
        readingThread_ = std::thread([this]() {
            while (!stopReading_) {
                std::vector<int32_t> buffer = ReadAudioData();
                if (!buffer.empty()) {
                    callback_(buffer); // Trigger the callback when data is available
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
                std::cerr << "Control open error for card " << card_name << ": " << snd_strerror(err) << std::endl;
                continue;
            }
    
            // Get card info
            if ((err = snd_ctl_card_info(handle, info)) < 0) {
                std::cerr << "Control card info error for card " << card_name << ": " << snd_strerror(err) << std::endl;
                snd_ctl_close(handle);
                continue;
            }
    
            // Print card name and driver info
            std::cout << "Card: " << snd_ctl_card_info_get_name(info) << std::endl;
            std::cout << "Driver: " << snd_ctl_card_info_get_driver(info) << std::endl;
    
            // List all PCM devices (both playback and capture)
            int pcm_device = -1;
            while (snd_ctl_pcm_next_device(handle, &pcm_device) >= 0 && pcm_device >= 0) {
                std::cout << "  PCM device: " << pcm_device << std::endl;
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
    std::function<void(const std::vector<int32_t>&)> callback_; // Store the callback function
    std::atomic<bool> stopReading_; // Flag to stop reading when destructor is called
    std::thread readingThread_; // Thread to read audio data asynchronously

    // Reads audio data into a buffer
    std::vector<int32_t> ReadAudioData() {
        std::vector<int32_t> buffer(numFrames_ * channels_);
        int framesRead = snd_pcm_readi(handle_, buffer.data(), numFrames_);
        if (framesRead < 0) {
            spdlog::get("Microphone Logger")->error("Error reading audio data: {}", snd_strerror(framesRead));
            snd_pcm_recover(handle_, framesRead, 1); // Try to recover on errors
        } else if (framesRead != static_cast<int>(numFrames_)) {
            spdlog::get("Microphone Logger")->warn("Warning: Partial read ({} frames read, expected {})", framesRead, numFrames_);
        }

        return buffer;
    }
};
