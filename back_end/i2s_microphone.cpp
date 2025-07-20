#include "i2s_microphone.h"

I2SMicrophone::I2SMicrophone(const std::string& targetDevice,
                             const std::string& signal_Name,
                             unsigned int sampleRate,
                             unsigned int numFrames,
                             _snd_pcm_format snd_pcm_format,
                             _snd_pcm_access snd_pcm_access,
                             bool allowResampling,
                             unsigned int latency,
                             std::shared_ptr<WebSocketServer> webSocketServer)
    : targetDevice_(targetDevice),
      signal_Name_(signal_Name),
      sampleRate_(sampleRate),
      numFrames_(numFrames),
      webSocketServer_(webSocketServer),
      inputSignalLeftChannel_(std::dynamic_pointer_cast<Signal<std::vector<int32_t>>>(signalManager_.getSharedSignalByName("Microphone Left Channel"))),
      inputSignalRightChannel_(std::dynamic_pointer_cast<Signal<std::vector<int32_t>>>(signalManager_.getSharedSignalByName("Microphone Right Channel"))),
      minDbSignal_(std::dynamic_pointer_cast<Signal<float>>(signalManager_.getSharedSignalByName("Min db"))),
      maxDbSignal_(std::dynamic_pointer_cast<Signal<float>>(signalManager_.getSharedSignalByName("Max db")))
{
    auto& handler = GuardDogHandler::getInstance();
    guarddog_ = handler.createGuardDog(5);

    logger_ = initializeLogger("I2s Microphone", spdlog::level::info);

    std::string deviceString = find_device(targetDevice_);
    if (deviceString.empty())
    {
        throw std::runtime_error("Target device not found: " + targetDevice_);
    }

    snd_pcm_t* raw_handle = nullptr;
    if (snd_pcm_open(&raw_handle, deviceString.c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0)
    {
        throw std::runtime_error("Failed to open I2S microphone: " + std::string(snd_strerror(errno)));
    }
    handle_.reset(raw_handle);

    logger_->info("Device {}: Opened successfully", targetDevice_);

    snd_pcm_hw_free(handle_.get());
    if (snd_pcm_set_params(handle_.get(), snd_pcm_format, snd_pcm_access, 2, sampleRate_, allowResampling, latency) < 0)
    {
        throw std::runtime_error("Failed to set ALSA parameters: " + std::string(snd_strerror(errno)));
    }

    // Register callbacks
    if (inputSignalLeftChannel_)
    {
        inputSignalLeftChannel_->registerSignalValueCallback([](const std::vector<int32_t>& value, void* arg)
        {
            auto* self = static_cast<I2SMicrophone*>(arg);
            self->logger_->debug("Device {}: New Left Channel values", self->targetDevice_);
        }, this);
    }

    if (inputSignalRightChannel_)
    {
        inputSignalRightChannel_->registerSignalValueCallback([](const std::vector<int32_t>& value, void* arg)
        {
            auto* self = static_cast<I2SMicrophone*>(arg);
            self->logger_->debug("Device {}: New Right Channel values", self->targetDevice_);
        }, this);
    }

    if (minDbSignal_)
    {
        minDbSignal_->registerSignalValueCallback([](const float& value, void* arg)
        {
            auto* self = static_cast<I2SMicrophone*>(arg);
            self->logger_->debug("Device {}: New Min db value", self->targetDevice_);
        }, this);
    }

    if (maxDbSignal_)
    {
        maxDbSignal_->registerSignalValueCallback([](const float& value, void* arg)
        {
            auto* self = static_cast<I2SMicrophone*>(arg);
            self->logger_->debug("Device {}: New Max db value", self->targetDevice_);
        }, this);
    }
}

I2SMicrophone::~I2SMicrophone()
{
    logger_->info("Device {}: Destructor called", targetDevice_);
    stopReading();

    if (inputSignalLeftChannel_)
    {
        inputSignalLeftChannel_->unregisterSignalValueCallbackByArg(this);
    }
    if (inputSignalRightChannel_)
    {
        inputSignalRightChannel_->unregisterSignalValueCallbackByArg(this);
    }
    if (minDbSignal_)
    {
        minDbSignal_->unregisterSignalValueCallbackByArg(this);
    }
    if (maxDbSignal_)
    {
        maxDbSignal_->unregisterSignalValueCallbackByArg(this);
    }

    logger_->info("Device {}: Destroyed", targetDevice_);
}

std::vector<int32_t> I2SMicrophone::readAudioData()
{
    logger_->debug("Device {}: Reading audio data", targetDevice_);
    std::vector<int32_t> buffer(numFrames_ * 2);

    int framesRead = snd_pcm_readi(handle_.get(), buffer.data(), numFrames_);
    if (framesRead < 0)
    {
        logger_->error("Device {}: Read error: {}", targetDevice_, snd_strerror(framesRead));
        if (snd_pcm_recover(handle_.get(), framesRead, 1) < 0)
        {
            logger_->error("Device {}: Recovery failed, resetting stream", targetDevice_);
            snd_pcm_prepare(handle_.get());
        }
    }
    else if (framesRead != static_cast<int>(numFrames_))
    {
        logger_->warn("Device {}: Partial read: {} frames read, expected {}", targetDevice_, framesRead, numFrames_);
    }

    return buffer;
}

void I2SMicrophone::startReadingMicrophone()
{
    logger_->debug("Device {}: Starting microphone reading", targetDevice_);
    stopReading();

    GuardDogHandler::getInstance().startMonitoringGuardDog(guarddog_);
    stopReading_ = false;

    readingThread_ = std::thread([this]()
    {
        while (!stopReading_)
        {
            guarddog_->feed();
            auto buffer = readAudioData();
            if (!buffer.empty())
            {
                splitAudioData(buffer);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

void I2SMicrophone::startReadingSineWave(double frequency)
{
    stopReading();
    stopReading_ = false;

    sineWaveThread_ = std::thread([this, frequency]()
    {
        double phase = 0.0;
        double phaseIncrement = 2.0 * M_PI * frequency / sampleRate_;

        while (!stopReading_)
        {
            std::vector<int32_t> buffer(numFrames_);
            for (size_t i = 0; i < numFrames_; ++i)
            {
                buffer[i] = static_cast<int32_t>(std::sin(phase) * INT32_MAX * 0.5); // Safer amplitude
                phase += phaseIncrement;
                if (phase >= 2.0 * M_PI)
                {
                    phase -= 2.0 * M_PI;
                }
            }
            if (inputSignalLeftChannel_) inputSignalLeftChannel_->setValue(buffer);
            if (inputSignalRightChannel_) inputSignalRightChannel_->setValue(buffer);

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

void I2SMicrophone::stopReading()
{
    stopReading_ = true;

    if (sineWaveThread_.joinable())
    {
        sineWaveThread_.join();
    }

    if (readingThread_.joinable())
    {
        // Interrupt blocking reads by dropping stream
        if (handle_)
        {
            snd_pcm_drop(handle_.get());
        }

        readingThread_.join();

        if (guarddog_)
        {
            GuardDogHandler::getInstance().stopMonitoringGuardDog(guarddog_);
        }
    }
}

void I2SMicrophone::splitAudioData(const std::vector<int32_t>& buffer)
{
    logger_->debug("Device {}: Splitting audio data", targetDevice_);
    std::vector<int32_t> leftChannel(numFrames_);
    std::vector<int32_t> rightChannel(numFrames_);

    for (size_t i = 0; i < numFrames_; ++i)
    {
        leftChannel[i] = buffer[i * 2];
        rightChannel[i] = buffer[i * 2 + 1];
    }

    if (inputSignalLeftChannel_) inputSignalLeftChannel_->setValue(leftChannel);
    if (inputSignalRightChannel_) inputSignalRightChannel_->setValue(rightChannel);
}

std::string I2SMicrophone::find_device(const std::string& targetDevice)
{
    snd_ctl_t* handle;
    snd_ctl_card_info_t* info;
    int card = -1;
    int err;

    snd_ctl_card_info_alloca(&info);

    while (snd_card_next(&card) >= 0 && card >= 0)
    {
        std::string card_name = "hw:" + std::to_string(card);

        if ((err = snd_ctl_open(&handle, card_name.c_str(), 0)) < 0)
        {
            logger_->error("Control open error for {}: {}", card_name, snd_strerror(err));
            continue;
        }

        if ((err = snd_ctl_card_info(handle, info)) < 0)
        {
            logger_->error("Card info error for {}: {}", card_name, snd_strerror(err));
            snd_ctl_close(handle);
            continue;
        }

        int pcm_device = -1;
        bool found = false;
        while (snd_ctl_pcm_next_device(handle, &pcm_device) >= 0 && pcm_device >= 0)
        {
            if (!found)
            {
                found = true;
                break;
            }
        }

        if (targetDevice == snd_ctl_card_info_get_name(info) && found)
        {
            std::string deviceString = "plug" + card_name + "," + std::to_string(pcm_device);
            snd_ctl_close(handle);
            logger_->info("Found target device: {}", deviceString);
            return deviceString;
        }

        snd_ctl_close(handle);
    }

    logger_->error("Target device {} not found", targetDevice);
    return "";
}
