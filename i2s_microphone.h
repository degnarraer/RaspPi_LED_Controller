#pragma once
#include <alsa/asoundlib.h>
#include <vector>
#include <iostream>
#include <stdexcept>

class I2SMicrophone {
public:
    I2SMicrophone(const std::string& deviceName, unsigned int sampleRate, unsigned int channels, unsigned int bufferSize)
        : deviceName_(deviceName), sampleRate_(sampleRate), channels_(channels), bufferSize_(bufferSize) {
        if (snd_pcm_open(&handle_, deviceName.c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0) {
            throw std::runtime_error("Failed to open I2S microphone");
        }

        if (snd_pcm_set_params(handle_,
                               SND_PCM_FORMAT_S16_LE,
                               SND_PCM_ACCESS_RW_INTERLEAVED,
                               channels_,
                               sampleRate_,
                               1,  // Allow resampling
                               500000) < 0) {  // 0.5s latency
            throw std::runtime_error("Failed to set ALSA parameters");
        }
    }

    ~I2SMicrophone() {
        if (handle_) {
            snd_pcm_close(handle_);
        }
    }

    // Reads audio data into a buffer
    std::vector<int32_t> ReadAudioData() {
        std::vector<int32_t> buffer(bufferSize_ * channels_);
        int framesRead = snd_pcm_readi(handle_, buffer.data(), bufferSize_);

        if (framesRead < 0) {
            snd_pcm_recover(handle_, framesRead, 1); // Try to recover on errors
        } else if (framesRead != static_cast<int>(bufferSize_)) {
            std::cerr << "Warning: Partial read (" << framesRead << " frames)" << std::endl;
        }

        return buffer;
    }

private:
    std::string deviceName_;
    unsigned int sampleRate_;
    unsigned int channels_;
    unsigned int bufferSize_;
    snd_pcm_t* handle_ = nullptr;
};