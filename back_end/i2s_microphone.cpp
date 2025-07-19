#include "i2s_microphone.h"

I2SMicrophone::I2SMicrophone( const std::string& targetDevice
                            , const std::string& signal_Name
                            , unsigned int sampleRate
                            , unsigned int numFrames
                            , _snd_pcm_format snd_pcm_format
                            , _snd_pcm_access snd_pcm_access
                            , bool allowResampling
                            , unsigned int latency
                            , std::shared_ptr<WebSocketServer> webSocketServer)
    : targetDevice_(targetDevice)
    , signal_Name_(signal_Name)
    , sampleRate_(sampleRate)
    , numFrames_(numFrames)
    , webSocketServer_(webSocketServer)
    , stopReading_(false)
    , inputSignalLeftChannel_(std::dynamic_pointer_cast<Signal<std::vector<int32_t>>>(signalManager_.getSharedSignalByName("Microphone Left Channel")))
    , inputSignalRightChannel_(std::dynamic_pointer_cast<Signal<std::vector<int32_t>>>(signalManager_.getSharedSignalByName("Microphone Right Channel")))
    , minDbSignal_(std::dynamic_pointer_cast<Signal<float>>(signalManager_.getSharedSignalByName("Min db")))
    , maxDbSignal_(std::dynamic_pointer_cast<Signal<float>>(signalManager_.getSharedSignalByName("Max db")))
{
    auto& handler = GuardDogHandler::getInstance();
    guarddog_ = handler.createGuardDog(5);

    // Retrieve existing logger or create a new one
    logger_ = initializeLogger("I2s Microphone", spdlog::level::info);
    if (snd_pcm_open(&handle_, find_device(targetDevice).c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0)
    {
        throw std::runtime_error("Failed to open I2S microphone: " + std::string(snd_strerror(errno)));
    }
    else
    {
        logger_->info("Device {}: Opening device", targetDevice_);
        snd_pcm_hw_free(handle_);
        if (snd_pcm_set_params(handle_, snd_pcm_format, snd_pcm_access, 2, sampleRate_, allowResampling, latency ) < 0)
        {
            throw std::runtime_error("Failed to set ALSA parameters: " + std::string(snd_strerror(errno)));
        }
        else
        {
            logger_->info("Device {}: Opened", targetDevice_);
        }
    }
    
    if (inputSignalLeftChannel_)
    {
        inputSignalLeftChannel_->registerSignalValueCallback([](const std::vector<int32_t>& value, void* arg)
        {
            I2SMicrophone* self = static_cast<I2SMicrophone*>(arg);
            self->logger_->debug("Device {}: Received new Left Channel values:", self->targetDevice_);
            for (int32_t v : value)
            {
                self->logger_->trace("Device {}: Value:{}", self->targetDevice_, v);
            }
        }, this);
    }
    
    if (inputSignalRightChannel_)
    {
        inputSignalRightChannel_->registerSignalValueCallback([](const std::vector<int32_t>& value, void* arg)
        {
            I2SMicrophone* self = static_cast<I2SMicrophone*>(arg);
            self->logger_->debug("Device {}: Received new Right Channel values:", self->targetDevice_);
            for (int32_t v : value)
            {
                self->logger_->trace("Device {}: Value:{}", self->targetDevice_, v);
            }
        }, this);
    }

    if (minDbSignal_)
    {
        minDbSignal_->registerSignalValueCallback([](const float& value, void* arg)
        {
            I2SMicrophone* self = static_cast<I2SMicrophone*>(arg);
            self->logger_->debug("Device {}: Received new Min Microphone Limit values:", self->targetDevice_);
        }, this);
    }

    if (maxDbSignal_)
    {
        maxDbSignal_->registerSignalValueCallback([](const float& value, void* arg)
        {
            I2SMicrophone* self = static_cast<I2SMicrophone*>(arg);
            self->logger_->debug("Device {}: Received new Max Microphone Limit values:", self->targetDevice_);
        }, this);
    }
}

I2SMicrophone::~I2SMicrophone()
{
    logger_->info("Device {}: I2SMicrophone destructor called", targetDevice_);
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

    if (handle_)
    {
        snd_pcm_close(handle_);
    }

    logger_->info("Device {}: I2SMicrophone destroyed", targetDevice_);
}

std::vector<int32_t> I2SMicrophone::readAudioData()
{
    logger_->debug("Device {}: ReadAudioData: Start", targetDevice_);
    std::vector<int32_t> buffer(numFrames_ * 2);
    int framesRead = snd_pcm_readi(handle_, buffer.data(), numFrames_);
    if (framesRead < 0)
    {
        logger_->error("Device {}: ReadAudioData: Error reading audio data: {}", targetDevice_, snd_strerror(framesRead));
        if (snd_pcm_recover(handle_, framesRead, 1) < 0)
        {
            logger_->error("Device {}: ReadAudioData: Failed to recover from error: {} Resetting Stream.", targetDevice_, snd_strerror(framesRead));
            snd_pcm_prepare(handle_);  
        }
        else
        {
            logger_->debug("Device {}: ReadAudioData: Recovered from error", targetDevice_);
        }
    } 
    else if (framesRead != static_cast<int>(numFrames_))
    {
        logger_->warn("Device {}: ReadAudioData: Partial read ({} frames read, expected {})", targetDevice_, framesRead, numFrames_);
    }
    else
    {
        logger_->debug("Device {}: ReadAudioData: Complete", targetDevice_);
    }
    return buffer;
}

void I2SMicrophone::startReadingMicrophone()
{

    logger_->debug("Device {}: StartReading", targetDevice_);
    GuardDogHandler::getInstance().startMonitoringGuardDog(guarddog_);
    stopReading();
    stopReading_ = false;
    readingThread_ = std::thread([this]()
    {
        while (!stopReading_)
        {
            this->guarddog_->feed();
            std::vector<int32_t> buffer = readAudioData();
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
                buffer[i] = static_cast<int32_t>(std::sin(phase) * INT32_MAX);
                phase += phaseIncrement;
                if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
            }
            inputSignalLeftChannel_->setValue(buffer);
            inputSignalRightChannel_->setValue(buffer);
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
        readingThread_.join();
        if(guarddog_)
        {
            GuardDogHandler::getInstance().stopMonitoringGuardDog(guarddog_);
        }
    }
}

void I2SMicrophone::splitAudioData(const std::vector<int32_t>& buffer)
{
    logger_->debug("Device {}: Audio data split started", targetDevice_);
    std::vector<int32_t> leftChannel(numFrames_);
    std::vector<int32_t> rightChannel(numFrames_);

    for (size_t i = 0; i < numFrames_; ++i)
    {
        leftChannel[i] = buffer[i * 2];
        rightChannel[i] = buffer[i * 2 + 1];
    }

    if (inputSignalLeftChannel_)
    {
        inputSignalLeftChannel_->setValue(leftChannel);
    }
    if (inputSignalRightChannel_)
    {
        inputSignalRightChannel_->setValue(rightChannel);
    }
    logger_->debug("Device {}: Audio data split complete", targetDevice_);
}

std::string I2SMicrophone::find_device(std::string targetDevice)
{
    snd_ctl_t *handle;
    snd_ctl_card_info_t *info;
    int card = -1;
    int err;

    snd_ctl_card_info_alloca(&info);

    // Iterate over all sound cards
    while (snd_card_next(&card) >= 0 && card >= 0)
    {
        std::string card_name = "hw:" + std::to_string(card);

        // Open control interface for the card
        if ((err = snd_ctl_open(&handle, card_name.c_str(), 0)) < 0)
        {
            logger_->error("Control open error for card {} : {}", card_name, snd_strerror(err));
            continue;
        }

        // Get card info
        if ((err = snd_ctl_card_info(handle, info)) < 0)
        {
            logger_->error("Control card info error for card {} : {}", card_name, snd_strerror(err));
            snd_ctl_close(handle);
            continue;
        }

        logger_->info("Card: {}" , snd_ctl_card_info_get_name(info));
        logger_->info("Driver: {}", snd_ctl_card_info_get_driver(info));

        // Find the first valid PCM device
        int pcm_device = -1;
        bool found = false;
        while (snd_ctl_pcm_next_device(handle, &pcm_device) >= 0 && pcm_device >= 0)
        {
            logger_->info("PCM device: {}", pcm_device);
            if (!found)
            {
                found = true;
                break;
            }
        }

        snd_ctl_close(handle);
        if(targetDevice == snd_ctl_card_info_get_name(info))
        {
            if (found) {
                std::string resultingDevice = "plug" + card_name + "," + std::to_string(pcm_device);
                logger_->info("Found Target Card: {}", resultingDevice);
                return resultingDevice;
            }
        }
    }
    return "";
}
