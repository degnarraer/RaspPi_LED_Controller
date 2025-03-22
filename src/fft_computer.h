#include "kiss_fft.h"
#include <vector>
#include <iostream>
#include <spdlog/spdlog.h>
#include "ring_buffer.h"
#include "signal.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>

enum class ChannelType {
    Mono,
    Left,
    Right
};

class FFTComputer {
public:
    FFTComputer(std::string name, size_t bufferSize, unsigned int sampleRate)
        : name_(name), bufferSize_(bufferSize), sampleRate_(sampleRate), stopFlag_(false) {
        
        fft_ = kiss_fft_alloc(bufferSize, 0, nullptr, nullptr);
        if (!fft_) {
            throw std::runtime_error("Failed to allocate memory for FFT.");
        }
        fftOutput_.resize(bufferSize_);
        
        registerCallbacks();
        
        fftThread_ = std::thread(&FFTComputer::processQueue, this);
    }

    ~FFTComputer() {
        stopFlag_ = true;
        cv_.notify_all();
        if (fftThread_.joinable()) {
            fftThread_.join();
        }

        unregisterCallbacks();
        if (fft_) {
            free(fft_);
        }
    }

    void addData(const std::vector<int32_t>& data, ChannelType channel) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            dataQueue_.emplace(data, channel);
        }
        cv_.notify_one();
    }

    void registerFFTCallback(const std::function<void(const std::vector<float>&, ChannelType)>& callback) {
        fftCallback_ = callback;
    }

private:
    struct DataPacket {
        std::vector<int32_t> data;
        ChannelType channel;
        DataPacket(std::vector<int32_t> d, ChannelType c) : data(std::move(d)), channel(c) {}
    };

    size_t bufferSize_;
    unsigned int sampleRate_;
    kiss_fft_cfg fft_;
    std::vector<kiss_fft_cpx> fftOutput_;
    std::function<void(const std::vector<float>&, ChannelType)> fftCallback_;

    std::string name_;
    std::atomic<bool> stopFlag_;
    std::thread fftThread_;
    std::mutex queueMutex_;
    std::condition_variable cv_;
    std::queue<DataPacket> dataQueue_;

    void registerCallbacks() {
        auto callback = [](const std::vector<int32_t>& value, void* arg, ChannelType channel) {
            FFTComputer* self = static_cast<FFTComputer*>(arg);
            spdlog::get("FFT Computer Logger")->debug("Device {}: Received {} channel values:", self->name_, static_cast<int>(channel));
            self->addData(value, channel);
        };

        SignalManager::GetInstance().GetSignal<std::vector<int32_t>>("Microphone")->RegisterCallback(
            [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Mono); }, this);
        SignalManager::GetInstance().GetSignal<std::vector<int32_t>>("Microphone_Left_Channel")->RegisterCallback(
            [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Left); }, this);
        SignalManager::GetInstance().GetSignal<std::vector<int32_t>>("Microphone_Right_Channel")->RegisterCallback(
            [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Right); }, this);
    }

    void unregisterCallbacks() {
        SignalManager::GetInstance().GetSignal<std::vector<int32_t>>("Microphone")->UnregisterCallbackByArg(this);
        SignalManager::GetInstance().GetSignal<std::vector<int32_t>>("Microphone_Left_Channel")->UnregisterCallbackByArg(this);
        SignalManager::GetInstance().GetSignal<std::vector<int32_t>>("Microphone_Right_Channel")->UnregisterCallbackByArg(this);
    }

    void processQueue() {
        while (!stopFlag_) {
            DataPacket dataPacket({}, ChannelType::Mono);

            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                cv_.wait(lock, [this] { return !dataQueue_.empty() || stopFlag_; });
                if (stopFlag_) break;
                dataPacket = std::move(dataQueue_.front());
                dataQueue_.pop();
            }

            processFFT(dataPacket);
        }
    }

    void processFFT(const DataPacket& dataPacket) {
        std::vector<kiss_fft_cpx> inputData(bufferSize_);
        for (size_t i = 0; i < std::min(bufferSize_, dataPacket.data.size()); ++i) {
            inputData[i].r = static_cast<float>(dataPacket.data[i]);
            inputData[i].i = 0;
        }

        kiss_fft(fft_, inputData.data(), fftOutput_.data());

        std::vector<float> magnitudes(bufferSize_);
        for (size_t i = 0; i < bufferSize_; ++i) {
            magnitudes[i] = sqrt(fftOutput_[i].r * fftOutput_[i].r + fftOutput_[i].i * fftOutput_[i].i);
        }

        if (fftCallback_) {
            fftCallback_(magnitudes, dataPacket.channel);
        }

        spdlog::get("FFT Computer Logger")->info("FFT Computed: {} bins for channel {}.", bufferSize_, static_cast<int>(dataPacket.channel));
    }
};
