#include "i2s_microphone.h"

I2SMicrophone::I2SMicrophone( const std::string& targetDevice
                            , const std::string& signal_Name
                            , unsigned int sampleRate
                            , unsigned int channels
                            , unsigned int numFrames
                            , _snd_pcm_format snd_pcm_format
                            , _snd_pcm_access snd_pcm_access
                            , bool allowResampling
                            , unsigned int latency
                            , std::shared_ptr<WebSocketServer> webSocketServer)
    : targetDevice_(targetDevice)
    , signal_Name_(signal_Name)
    , sampleRate_(sampleRate)
    , channels_(channels)
    , numFrames_(numFrames)
    , webSocketServer_(webSocketServer)
    , stopReading_(false)
{
    // Retrieve existing logger or create a new one
    logger_ = InitializeLogger("I2s Microphone", spdlog::level::info);
    if (snd_pcm_open(&handle_, find_device(targetDevice).c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0)
    {
        throw std::runtime_error("Failed to open I2S microphone: " + std::string(snd_strerror(errno)));
    }
    else
    {
        logger_->info("Device {}: Opening device", targetDevice_);
        snd_pcm_hw_free(handle_);
        if (snd_pcm_set_params(handle_, snd_pcm_format, snd_pcm_access, channels_, sampleRate_, allowResampling, latency ) < 0)
        {
            throw std::runtime_error("Failed to set ALSA parameters: " + std::string(snd_strerror(errno)));
        }
        else
        {
            logger_->info("Device {}: Opened", targetDevice_);
        }
    }

    microphoneSignalCallback_ = [](const std::vector<int32_t>& value, void* arg)
    {
        I2SMicrophone* self = static_cast<I2SMicrophone*>(arg);
        self->logger_->debug("Device {}: Received new values:", self->targetDevice_);
        for (int32_t v : value)
        {
            self->logger_->trace("Device {}: Value:{}", self->targetDevice_, v);
        }
    };
    SignalManager::GetInstance().CreateSignal<std::vector<int32_t>>("Microphone")->RegisterCallback(microphoneSignalCallback_, this);
    
    microphoneLeftChannelSignalCallback_ = [](const std::vector<int32_t>& value, void* arg)
    {
        I2SMicrophone* self = static_cast<I2SMicrophone*>(arg);
        self->logger_->debug("Device {}: Received new Left Channel values:", self->targetDevice_);
        for (int32_t v : value)
        {
            self->logger_->trace("Device {}: Value:{}", self->targetDevice_, v);
        }
    };
    SignalManager::GetInstance().CreateSignal<std::vector<int32_t>>("Microphone Left Channel")->RegisterCallback(microphoneLeftChannelSignalCallback_, this);

    microphoneRightChannelSignalCallback_ = [](const std::vector<int32_t>& value, void* arg)
    {
        I2SMicrophone* self = static_cast<I2SMicrophone*>(arg);
        self->logger_->debug("Device {}: Received new Right Channel values:", self->targetDevice_);
        for (int32_t v : value)
        {
            self->logger_->trace("Device {}: Value:{}", self->targetDevice_, v);
        }
    };
    SignalManager::GetInstance().CreateSignal<std::vector<int32_t>>("Microphone Right Channel")->RegisterCallback(microphoneRightChannelSignalCallback_, this);
}

I2SMicrophone::~I2SMicrophone()
{
    StopReading();
    
    {
        auto signal = dynamic_cast<Signal<int16_t>*>(SignalManager::GetInstance().GetSignalByName("Microphone"));
        if (signal)
        {
            signal->UnregisterCallbackByArg(this);
        }
    }
    {
        auto signal = dynamic_cast<Signal<int16_t>*>(SignalManager::GetInstance().GetSignalByName("Microphone Left Channel"));
        if (signal)
        {
            signal->UnregisterCallbackByArg(this);
        }
    }
    {
        auto signal = dynamic_cast<Signal<int16_t>*>(SignalManager::GetInstance().GetSignalByName("Microphone Right Channel"));
        if (signal)
        {
            signal->UnregisterCallbackByArg(this);
        }
    }

    if (handle_)
    {
        snd_pcm_close(handle_);
    }
}

std::vector<int32_t> I2SMicrophone::ReadAudioData()
{
    logger_->debug("Device {}: ReadAudioData: Start", targetDevice_);
    std::vector<int32_t> buffer(numFrames_ * channels_);
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

    // Start reading audio data in a separate thread and call all registered callbacks when buffer is full
void I2SMicrophone::StartReadingMicrophone()
{
    logger_->debug("Device {}: StartReading", targetDevice_);
    StopReading();
    stopReading_ = false;
    readingThread_ = std::thread([this]()
    {
        while (!stopReading_)
        {
            std::vector<int32_t> buffer = ReadAudioData();
            if (!buffer.empty())
            {
                switch(channels_)
                {
                    case 1:
                    {
                        auto signal = dynamic_cast<Signal<std::vector<int32_t>>*>(SignalManager::GetInstance().GetSignalByName("Microphone"));
                        if (signal)
                        {
                            signal->SetValue(buffer);
                        }
                    }
                    break;
                    case 2:
                        SplitAudioData(buffer);
                    break;
                    default:
                        logger_->error("Device {}: Invalid channel config.", targetDevice_);
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Small delay to prevent high CPU usage
        }
    });
}

void I2SMicrophone::StartReadingSineWave(double frequency)
{
    StopReading();
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
            auto signal = dynamic_cast<Signal<std::vector<int32_t>>*>(SignalManager::GetInstance().GetSignalByName("Microphone"));
            if (signal)
            {
                signal->SetValue(buffer);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

void I2SMicrophone::StopReading()
{

    stopReading_ = true;
    if (sineWaveThread_.joinable())
    {
        sineWaveThread_.join();
    }        
    if (readingThread_.joinable())
    {
        readingThread_.join();
    }
}

void I2SMicrophone::SplitAudioData(const std::vector<int32_t>& buffer)
{
    if (channels_ != 2)
    {
        return;
    }
    logger_->debug("Device {}: Audio data split started", targetDevice_);
    // Reserve space for left and right channels
    std::vector<int32_t> leftChannel(numFrames_);
    std::vector<int32_t> rightChannel(numFrames_);

    // Split interleaved stereo data into separate left and right channels
    for (size_t i = 0; i < numFrames_; ++i)
    {
        // Extract the left and right samples (assuming buffer is interleaved)
        leftChannel.push_back(buffer[i * channels_]);         // Even indices are left channel
        rightChannel.push_back(buffer[i * channels_ + 1]);    // Odd indices are right channel
    }
    auto leftChannelSignal = dynamic_cast<Signal<std::vector<int32_t>>*>(SignalManager::GetInstance().GetSignalByName("Microphone Left Channel"));
    if (leftChannelSignal)
    {
        leftChannelSignal->SetValue(leftChannel);
    }
    auto rightChannelSignal = dynamic_cast<Signal<std::vector<int32_t>>*>(SignalManager::GetInstance().GetSignalByName("Microphone Right Channel"));
    if (rightChannelSignal)
    {
        rightChannelSignal->SetValue(rightChannel);
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
