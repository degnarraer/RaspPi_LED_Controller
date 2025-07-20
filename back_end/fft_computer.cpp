#include "fft_computer.h"



FFTComputer::FFTComputer( const std::string name
            , const std::string input_signal_name
            , const std::string output_signal_name
            , size_t fft_size
            , unsigned int sampleRate
            , int32_t maxValue
            , std::shared_ptr<WebSocketServer> webSocketServer)
    : name_(name)
    , input_signal_name_(input_signal_name)
    , output_signal_name_(output_signal_name)
    , fft_size_(fft_size)
    , sampleRate_(sampleRate)
    , maxValue_(maxValue)
    , webSocketServer_(webSocketServer)
    , stopFlag_(false)
    , minRenderFrequencySignal_(std::dynamic_pointer_cast<Signal<float>>(SignalManager::getInstance().getSharedSignalByName("Minimum Render Frequency")))
    , maxRenderFrequencySignal_(std::dynamic_pointer_cast<Signal<float>>(SignalManager::getInstance().getSharedSignalByName("Maximum Render Frequency")))
{
    // Retrieve existing logger_ or create a new one
    logger_ = initializeLogger("FFT Computer", spdlog::level::info);
    inputSignalLeftChannel_ = dynamic_cast<Signal<std::vector<int32_t>>*>(SignalManager::getInstance().getSignalByName(input_signal_name_ + " Left Channel"));
    if(!inputSignalLeftChannel_)throw std::runtime_error("Failed to get signal: " + input_signal_name_ + " Left Channel");
    inputSignalRightChannel_ = dynamic_cast<Signal<std::vector<int32_t>>*>(SignalManager::getInstance().getSignalByName(input_signal_name_ + " Right Channel"));
    if(!inputSignalRightChannel_)throw std::runtime_error("Failed to get signal: " + input_signal_name_ + " Right Channel");
    fft_ = kiss_fft_alloc(fft_size_, 0, nullptr, nullptr);
    if (!fft_)
    {
        throw std::runtime_error("Failed to allocate memory for FFT.");
    }
    fftOutput_.resize(fft_size_);        
    registerCallbacks();
    fftThread_ = std::thread([this]() { this->processQueue(); });

    minDbSignal_ = std::dynamic_pointer_cast<Signal<float>>(SignalManager::getInstance().getSharedSignalByName("Min db"));
    if (minDbSignal_)
    {
        minDbSignal_->setValue(minDbValue_);
        minDbSignal_->registerSignalValueCallback([](const float& value, void* arg)
        {
            FFTComputer* self = static_cast<FFTComputer*>(arg);
            self->minDbValue_ = value;
            self->logger_->info("FFT Computer: Received new Min Db value: {}", value);
        }, this);
    }
    else
    {
        logger_->warn("FFT Computer: Min db signal not found, using default value: {}", minDbValue_);
    }

    maxDbSignal_ = std::dynamic_pointer_cast<Signal<float>>(SignalManager::getInstance().getSharedSignalByName("Max db"));
    if (maxDbSignal_)
    {
        maxDbSignal_->setValue(maxDbValue_);
        maxDbSignal_->registerSignalValueCallback([](const float& value, void* arg)
        {
            FFTComputer* self = static_cast<FFTComputer*>(arg);
            self->maxDbValue_ = value;
            self->logger_->info("FFT Computer: Received new Max Db value: {}", value);
        }, this);
    }
    else
    {
        logger_->warn("FFT Computer: Max db signal not found, using default value: {}", maxDbValue_);
    }
    
    auto minRenderFrequencySignal = minRenderFrequencySignal_.lock();
    if (minRenderFrequencySignal)
    {
        logger_->info("Minimum Render Frequency signal initialized successfully.");
        minRenderFrequencySignal->registerSignalValueCallback([this](const float& value, void* arg) {
            std::lock_guard<std::mutex> lock(this->mutex_);
            this->logger_->info("Minimum Render Frequency Signal Callback: {}", value);
            this->minRenderFrequency_ = value;
        }, this);
    }
    else
    {
        logger_->warn("Minimum Render Frequency Signal not found, using default value: 0.0f");
        minRenderFrequency_ = 0.0f; // Default initialization
    }

    auto maxRenderFrequencySignal = maxRenderFrequencySignal_.lock();
    if (maxRenderFrequencySignal)
    {
        logger_->info("Maximum Render Frequency signal initialized successfully.");
        maxRenderFrequencySignal->registerSignalValueCallback([this](const float& value, void* arg) {
            std::lock_guard<std::mutex> lock(this->mutex_);
            this->logger_->info("Maximum Render Frequency Signal Callback: {}", value);
            this->maxRenderFrequency_ = value;
        }, this);
    }
    else
    {
        logger_->warn("Maximum Render Frequency Signal not found, using default value: 24000.0f");
        maxRenderFrequency_ = sampleRate_ / 2.0f; // Default initialization
    }

}

void FFTComputer::addData(const std::vector<int32_t>& data, ChannelType channel)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        dataQueue_.emplace(data, channel);
    }
    cv_.notify_one();
}

void FFTComputer::registerFFTCallback(const std::function<void(const std::vector<float>&, ChannelType)>& callback)
{
    fftCallback_ = callback;
}

std::array<float, 32> FFTComputer::GetFFTBandCenters() const
{
    return ISO_32_BAND_CENTERS;
};

std::string FFTComputer::channelTypeToString(ChannelType channel)
{
    switch (channel)
    {
        case ChannelType::Left:
            return "Left";
        case ChannelType::Right:
            return "Right";
        default:
            return "Unknown";
            
    }
}

std::vector<std::string> FFTComputer::GetIsoBandLabels()
{
    std::vector<std::string> labels;
    labels.reserve(ISO_32_BAND_CENTERS.size());

    std::transform(
        ISO_32_BAND_CENTERS.begin(),
        ISO_32_BAND_CENTERS.end(),
        std::back_inserter(labels),
        [](float f)
        {
            std::ostringstream oss;
            oss.precision(1);
            oss << std::fixed << f;
            return oss.str();
        }
    );

    return labels;
}

void FFTComputer::registerCallbacks()
{
    auto callback = [this](const std::vector<int32_t>& value, void* arg, ChannelType channel)
    {
        FFTComputer* self = static_cast<FFTComputer*>(arg);
        spdlog::get("FFT Computer")->debug("Device {}: Received {} channel values:", self->name_, this->channelTypeToString(channel));
        self->addData(value, channel);
    };

    inputSignalLeftChannel_->registerSignalValueCallback( [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Left); }, this );
    inputSignalRightChannel_->registerSignalValueCallback( [callback](const std::vector<int32_t>& value, void* arg) { callback(value, arg, ChannelType::Right); }, this );
}

void FFTComputer::unregisterCallbacks()
{
    inputSignalLeftChannel_->unregisterSignalValueCallbackByArg(this);
    inputSignalRightChannel_->unregisterSignalValueCallbackByArg(this);
}

void FFTComputer::processQueue()
{
    std::vector<int32_t> monoBuffer;
    std::vector<int32_t> leftBuffer;
    std::vector<int32_t> rightBuffer;
    const size_t minimumStepSize = 512;
    const size_t requiredSamples = fft_size_;
    const size_t overlapSamples = std::min(fft_size_ - 512, std::max(minimumStepSize, fft_size_));
    const size_t nonOverlappingSamples = requiredSamples - overlapSamples;

    while (!stopFlag_)
    {
        DataPacket dataPacket;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !dataQueue_.empty() || stopFlag_; });
            if (stopFlag_) break;
            dataPacket = std::move(dataQueue_.front());
            dataQueue_.pop();
        }

        // Select buffer based on channel
        std::vector<int32_t>* buffer = nullptr;
        switch (dataPacket.channel)
        {
            case ChannelType::Left: buffer = &leftBuffer; break;
            case ChannelType::Right: buffer = &rightBuffer; break;
            default: continue;
        }

        buffer->insert(buffer->end(), std::make_move_iterator(dataPacket.data.begin()), std::make_move_iterator(dataPacket.data.end()));

        while (buffer->size() >= requiredSamples)
        {
            std::vector<int32_t> fftData(buffer->begin(), buffer->begin() + requiredSamples);
            buffer->erase(buffer->begin(), buffer->begin() + nonOverlappingSamples);
            processFFT({ std::move(fftData), dataPacket.channel });
        }
    }
}

void FFTComputer::processFFT(const DataPacket& dataPacket)
{
    const auto& data = dataPacket.data;
    const auto channel = dataPacket.channel;

    if (!fft_)
    {
        logger_->error("FFT not initialized");
        return;
    }

    // --- Calculate total power BEFORE FFT ---

    float sumSquares = 0.0f;
    size_t n = data.size();

    for (size_t i = 0; i < n; ++i)
    {
        float sample = static_cast<float>(data[i]) / static_cast<float>(maxValue_);
        sumSquares += sample * sample;
    }

    float rms = (n > 0) ? std::sqrt(sumSquares / n) : 0.0f;
    float safeRms = std::max(rms, 1e-12f);

    float totalPowerDb = 20.0f * std::log10(safeRms) + micOffsetDb;
    totalPowerDb = std::max(totalPowerDb, minDbValue_);

    float denom = maxDbValue_ - minDbValue_;
    if (denom <= 0.0f) denom = 1.0f; // avoid div by zero

    float totalNormalizedPower = (totalPowerDb - minDbValue_) / denom;
    totalNormalizedPower = std::clamp(totalNormalizedPower, 0.0f, 1.0f);

    // --- Prepare input for kiss_fft ---

    std::vector<kiss_fft_cpx> fftInput(fft_size_);
    for (size_t i = 0; i < fft_size_ && i < data.size(); ++i)
    {
        fftInput[i].r = static_cast<float>(data[i]) / static_cast<float>(maxValue_);
        fftInput[i].i = 0.0f;
    }

    // Zero pad
    for (size_t i = data.size(); i < fft_size_; ++i)
    {
        fftInput[i].r = 0.0f;
        fftInput[i].i = 0.0f;
    }

    // Perform FFT
    kiss_fft(fft_, fftInput.data(), fftOutput_.data());

    // Calculate magnitudes
    std::vector<float> magnitudes(fft_size_ / 2);
    for (size_t i = 0; i < fft_size_ / 2; ++i)
    {
        float real = fftOutput_[i].r / fft_size_;
        float imag = fftOutput_[i].i / fft_size_;
        magnitudes[i] = sqrtf(real * real + imag * imag) * sqrt2;
    }

    // Compute SAE bands (raw RMS amplitudes)
    std::vector<float> saeBands(32);
    BinData binData;
    computeSAEBands(magnitudes, saeBands);
    computeFFTBinData(magnitudes, binData);

    // Convert to SPL dB and normalized bands
    std::vector<float> splBands(32);
    std::vector<float> normalizedBands(32);

    for (size_t i = 0; i < saeBands.size(); ++i)
    {
        float amplitude = saeBands[i];

        // Defensive safe amplitude
        float safeAmplitude = std::max(amplitude, 1e-12f);

        // Convert to dB SPL
        float db = 20.0f * log10f(safeAmplitude) + micOffsetDb;

        // Defensive normalization denominator
        float denom = maxDbValue_ - minDbValue_;
        if (denom <= 0.0f) denom = 1.0f; // avoid div by zero

        // Normalize
        float normalized = (db - minDbValue_) / denom;
        float clampedNormalized = std::clamp(normalized, 0.0f, 1.0f);

        splBands[i] = db;
        normalizedBands[i] = clampedNormalized;

        logger_->trace("FFT band {}: amplitude={}, db={}, normalized={}, clamped normalized={}", i, amplitude, db, normalized, clampedNormalized);
    }

    std::string totalPowerStr = std::to_string(totalPowerDb);
    std::string totalNormalizedPowerStr = std::to_string(totalNormalizedPower);

    logger_->trace("FFT Computer {}: Channel {}: Total Power SPL: {}, Total Normalized Power: {}", name_, channelTypeToString(channel), totalPowerStr, totalNormalizedPowerStr);

    // Publish signals
    if (channel == ChannelType::Left)
    {
        leftChannelFFTOutputSPLSignal_->setValue(splBands);
        leftChannelFFTOutputNormalizedSignal_->setValue(normalizedBands);
        leftChannelPowerSPLSignal_->setValue(totalPowerStr);
        leftChannelPowerNormalizedSignal_->setValue(totalNormalizedPowerStr);
    }
    else
    {
        rightChannelFFTOutputSPLSignal_->setValue(splBands);
        rightChannelFFTOutputNormalizedSignal_->setValue(normalizedBands);
        rightChannelPowerSPLSignal_->setValue(totalPowerStr);
        rightChannelPowerNormalizedSignal_->setValue(totalNormalizedPowerStr);
    }

    logSAEBands(saeBands);
}

void FFTComputer::logSAEBands(std::vector<float>& saeBands) const
{
    std::string result;
    for (size_t i = 0; i < saeBands.size(); ++i)
    {
        if(i > 0) result += " ";
        result += fmt::format("{:.1f}", saeBands[i]);
    }
    logger_->trace("SAE Band Values: {}", result);
}

float FFTComputer::normalizeDb(float amplitude)
{
    float db = 20.0f * std::log10(amplitude + 1e-6f);
    float normalized = (db - minDbValue_) / (maxDbValue_ - minDbValue_);
    return std::clamp(normalized, 0.0f, 1.0f);
}

void FFTComputer::computeFFTBinData(const std::vector<float>& magnitudes, BinData& binData)
{
    float freqResolution = static_cast<float>(sampleRate_) / fft_size_;

    // Convert min/max frequency to bin range
    size_t minAllowedBin = static_cast<size_t>(std::floor(minRenderFrequency_ / freqResolution));
    size_t maxAllowedBin = static_cast<size_t>(std::ceil(maxRenderFrequency_ / freqResolution));
    logger_->trace("FFT Computer {}: minRenderFrequency_={} Hz, maxRenderFrequency_={} Hz, freqResolution={} Hz/bin, minAllowedBin={}, maxAllowedBin={}", name_, minRenderFrequency_, maxRenderFrequency_, freqResolution, minAllowedBin, maxAllowedBin);
    // Clamp to valid FFT bin indices
    minAllowedBin = std::min(minAllowedBin, fft_size_ - 1);
    maxAllowedBin = std::min(maxAllowedBin, fft_size_ - 1);

    // Initialize search
    float minValue = std::numeric_limits<float>::max();
    float maxValue = std::numeric_limits<float>::lowest();
    uint16_t minBinIndex = static_cast<uint16_t>(minAllowedBin);
    uint16_t maxBinIndex = static_cast<uint16_t>(minAllowedBin);

    for (size_t i = minAllowedBin; i <= maxAllowedBin; ++i)
    {
        if (magnitudes[i] < minValue)
        {
            minValue = magnitudes[i];
            minBinIndex = static_cast<uint16_t>(i);
        }
        if (magnitudes[i] > maxValue)
        {
            maxValue = magnitudes[i];
            maxBinIndex = static_cast<uint16_t>(i);
        }
    }

    // Populate binData
    binData.minBin = minBinIndex;
    binData.maxBin = maxBinIndex;
    binData.totalBins = static_cast<uint16_t>(maxAllowedBin - minAllowedBin + 1);
    binData.normalizedMinValue = minValue;
    binData.normalizedMaxValue = maxValue;
}

void FFTComputer::computeSAEBands(const std::vector<float>& magnitudes, std::vector<float>& saeBands)
{
    float freqResolution = static_cast<float>(sampleRate_) / fft_size_;

    saeBands.resize(32);
    for (size_t i = 0; i < ISO_32_BAND_CENTERS.size(); ++i)
    {
        float lowerFreq, upperFreq;

        if (i == 0)
        {
            lowerFreq = ISO_32_BAND_CENTERS[i] / sqrt2;
        }
        else
        {
            lowerFreq = (ISO_32_BAND_CENTERS[i - 1] + ISO_32_BAND_CENTERS[i]) / 2.0f;
        }

        if (i == ISO_32_BAND_CENTERS.size() - 1)
        {
            upperFreq = ISO_32_BAND_CENTERS[i] * sqrt2;
        }
        else
        {
            upperFreq = (ISO_32_BAND_CENTERS[i] + ISO_32_BAND_CENTERS[i + 1]) / 2.0f;
        }

        size_t binStart = static_cast<size_t>(std::floor(lowerFreq / freqResolution));
        size_t binEnd = static_cast<size_t>(std::ceil(upperFreq / freqResolution));

        // CLAMP TO magnitudes size (fft_size_/2)
        binStart = std::min(binStart, (fft_size_/2) - 1);
        binEnd = std::min(binEnd, (fft_size_/2) - 1);

        saeBands[i] = 0.0f;
        size_t count = 0;

        for (size_t j = binStart; j <= binEnd; ++j)
        {
            saeBands[i] += magnitudes[j] * magnitudes[j];
            ++count;
        }

        if (count > 0)
        {
            saeBands[i] = std::sqrt(saeBands[i] / static_cast<float>(count));
        }
        else
        {
            saeBands[i] = 0.0f;
        }
    }
}

double FFTComputer::getFFTFrequency(int binIndex)
{
    return (sampleRate_ / fft_size_) * binIndex;
}
