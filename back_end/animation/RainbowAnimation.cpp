#include "RainbowAnimation.h"

RainbowAnimation::RainbowAnimation(PixelGridSignal& grid)
    : PixelGridAnimation(grid, 100)
    , leftBinDataSignal_(std::dynamic_pointer_cast<Signal<BinData>>(SignalManager::getInstance().getSharedSignalByName("FFT Bands Left Bin Data")))
    , rightBinDataSignal_(std::dynamic_pointer_cast<Signal<BinData>>(SignalManager::getInstance().getSharedSignalByName("FFT Bands Right Bin Data")))
    , colorMappingTypeSignal_(std::dynamic_pointer_cast<Signal<ColorMappingType>>(SignalManager::getInstance().getSharedSignalByName("Color Mapping Type")))
    , logger_(initializeLogger("Rainbow Animation Logger", spdlog::level::info))
{    
    if (leftBinDataSignal_)
    {
        leftBinDataSignal_->registerSignalValueCallback([this](const BinData& value, void* arg) {
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

    if (rightBinDataSignal_)
    {
        rightBinDataSignal_->registerSignalValueCallback([this](const BinData& value, void* arg) {
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

    if (colorMappingTypeSignal_)
    {
        colorMappingTypeSignal_->registerSignalValueCallback([this](const ColorMappingType& value, void* arg) {
            std::lock_guard<std::mutex> lock(this->mutex_);
            this->logger_->debug("Color Mapping Type Signal Callback.");
            this->colorMappingType_ = value;
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
    RGB color = ColorMapper::normalizedToRGB(leftBinData_.maxBin, leftBinData_.totalBins, normalized);

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
