#include "kiss_fft.h"
#include <vector>
#include <iostream>
#include <spdlog/spdlog.h>
#include "ring_buffer.h"
#include "signal.h"

class FFTComputer {
public:
    FFTComputer(size_t bufferSize, unsigned int sampleRate)
        : bufferSize_(bufferSize), sampleRate_(sampleRate), ringBuffer_(bufferSize) {
        // Initialize the FFT configuration
        fft_ = kiss_fft_alloc(bufferSize, 0, nullptr, nullptr);  // 0 for forward FFT
        if (!fft_) {
            throw std::runtime_error("Failed to allocate memory for FFT.");
        }
        fftOutput_.resize(bufferSize_);
        
        microphoneSignal.RegisterCallback([](const std::vector<int32_t>& value, void* arg) {
            I2SMicrophone* self = static_cast<I2SMicrophone*>(arg);
            spdlog::get("FFT Computer Logger")->debug("Device {}: Received new values:", self->deviceName_);
            for (int32_t v : value) {
                spdlog::get("FFT Computer Logger")->trace("Device {}: Value:{}", self->deviceName_, v);
            }
        });
    }

    ~FFTComputer() {
        if (fft_) {
            free(fft_);
        }
    }

    // Add new audio data from the microphone to the buffer
    void addData(const std::vector<int32_t>& data) {
        for (auto sample : data) {
            ringBuffer_.push(static_cast<float>(sample));
        }

        // If the buffer is full, calculate the FFT
        if (ringBuffer_.available() >= bufferSize_) {
            computeFFT();
        }
    }

    // Register a callback to handle the FFT result
    void registerFFTCallback(const std::function<void(const std::vector<float>&)>& callback) {
        fftCallback_ = callback;
    }

private:
    size_t bufferSize_;
    unsigned int sampleRate_;
    RingBuffer<float> ringBuffer_;
    kiss_fft_cfg fft_;
    std::vector<kiss_fft_cpx> fftOutput_;
    std::function<void(const std::vector<float>&)> fftCallback_;
    Signal<std::vector<int32_t>> microphoneSignal = Signal<std::vector<int32_t>>("Microphone");

    // Perform the FFT calculation on the buffer
    void computeFFT() {
        std::vector<float> bufferData = ringBuffer_.get_all();

        // Prepare input data for KissFFT (convert float data to kiss_fft_cpx format)
        std::vector<kiss_fft_cpx> inputData(bufferSize_);
        for (size_t i = 0; i < bufferSize_; ++i) {
            inputData[i].r = bufferData[i]; // Real part
            inputData[i].i = 0;             // Imaginary part
        }

        // Perform the FFT
        kiss_fft(fft_, inputData.data(), fftOutput_.data());

        // Convert the result to magnitudes
        std::vector<float> magnitudes(bufferSize_);
        for (size_t i = 0; i < bufferSize_; ++i) {
            magnitudes[i] = sqrt(fftOutput_[i].r * fftOutput_[i].r + fftOutput_[i].i * fftOutput_[i].i);
        }

        // If a callback is registered, call it with the FFT magnitudes
        if (fftCallback_) {
            fftCallback_(magnitudes);
        }

        spdlog::get("FFT Computer Logger")->info("FFT Computed: {} bins.", bufferSize_);
    }
};
