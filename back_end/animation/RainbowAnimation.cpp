#include "RainbowAnimation.h"

RainbowAnimation::RainbowAnimation(PixelGridSignal& grid)
    : PixelGridAnimation(grid, 100)
    , leftBinDataSignal_(std::dynamic_pointer_cast<Signal<BinData>>(SignalManager::getInstance().getSharedSignalByName("FFT Computer Left Bin Data")))
    , rightBinDataSignal_(std::dynamic_pointer_cast<Signal<BinData>>(SignalManager::getInstance().getSharedSignalByName("FFT Computer Right Bin Data")))
    , colorMappingTypeSignal_(std::dynamic_pointer_cast<Signal<std::string>>(SignalManager::getInstance().getSharedSignalByName("Color Mapping Type")))
    , logger_(initializeLogger("Rainbow Animation Logger", spdlog::level::info))
{   
    auto leftBinDataSignal = leftBinDataSignal_.lock();
    if (leftBinDataSignal)
    {
        logger_->info("FFT Computer Left Bin Data signal initialized successfully.");
        leftBinDataSignal->registerSignalValueCallback([this](const BinData& value, void* arg) {
            std::lock_guard<std::mutex> lock(this->mutex_);
            this->logger_->debug("Left Bin Data Signal Callback.");
            this->leftBinData_ = value;
        }, this);
    }
    else
    {
        logger_->warn("Left Bin Data Signal not found, using default value.");
        leftBinData_ = {0, 0, 0, 0.0f, 0.0f}; // Default initialization
    }

    auto rightBinDataSignal = rightBinDataSignal_.lock();
    if (rightBinDataSignal)
    {
        logger_->info("FFT Computer Right Bin Data signal initialized successfully.");
        rightBinDataSignal->registerSignalValueCallback([this](const BinData& value, void* arg) {
            std::lock_guard<std::mutex> lock(this->mutex_);
            this->logger_->debug("Right Bin Data Signal Callback.");
            this->rightBinData_ = value;
        }, this);
    }
    else
    {
        logger_->warn("Right Bin Data Signal not found, using default value.");
        rightBinData_ = {0, 0, 0, 0.0f, 0.0f};
    }

    auto colorMappingTypeSignal = colorMappingTypeSignal_.lock();
    if (colorMappingTypeSignal)
    {
        logger_->info("Color Mapping Type signal initialized successfully.");
        colorMappingType_ = from_string<ColorMappingType>(colorMappingTypeSignal->getValue());
        colorMappingTypeSignal->registerSignalValueCallback([this](const std::string& value, void* arg) {
            std::lock_guard<std::mutex> lock(this->mutex_);
            this->logger_->info("Color Mapping Type Signal Callback: {}", value);
            try
            {
                this->colorMappingType_ = from_string<ColorMappingType>(value);
            }
            catch (const std::exception& e)
            {
                logger_->error("Invalid color mapping value '{}': {}", value, e.what());
            }
        }, this);
    }
    else
    {
        logger_->warn("Color Mapping Type Signal not found, using default value: Linear.");
        colorMappingType_ = ColorMappingType::Linear;
    }
}

void RainbowAnimation::AnimateFrame()
{
    std::lock_guard<std::mutex> lock(mutex_);

    const int width = grid_.getWidth();
    const int height = grid_.getHeight();

    // Normalize amplitude to [0, 1]
    float normalized = leftBinData_.normalizedMaxValue;

    // Get bright rainbow color

    RGB color = ColorMapper::normalizedToRGB(leftBinData_.maxBin, leftBinData_.totalBins, normalized, colorMappingType_);

    // Scroll all rows down
    for (int y = 0; y < height - 1; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            grid_.setPixel(x, y, grid_.getValue(x, y + 1));
        }
    }

    // Set the bottom row to the new color
    for (int x = 0; x < width; ++x)
    {
        grid_.setPixel(x, height - 1, color);
    }

    grid_.notify();
}
