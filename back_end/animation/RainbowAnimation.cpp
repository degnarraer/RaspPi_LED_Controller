#include "RainbowAnimation.h"

RainbowAnimation::RainbowAnimation(PixelGridSignal& grid)
    : PixelGridAnimation(grid, 100)
{    
    auto leftBinDataBase_ = SignalManager::getInstance().getSharedSignalByName("FFT Bands Left Bin Data");
    auto rightBinDataBase_ = SignalManager::getInstance().getSharedSignalByName("FFT Bands Right Bin Data");
    leftBinDataSignal_ = std::dynamic_pointer_cast<Signal<BinData>>(leftBinDataBase_);
    rightBinDataSignal_ = std::dynamic_pointer_cast<Signal<BinData>>(rightBinDataBase_);
    if (leftBinDataSignal_)
    {
        leftBinDataSignal_->registerSignalValueCallback([this](const BinData& value, void* arg) {
            OnLeftBinDataUpdate(value, nullptr);
        }, this);
    }

    if (rightBinDataSignal_)
    {
        rightBinDataSignal_->registerSignalValueCallback([this](const BinData& value, void* arg) {
            OnRightBinDataUpdate(value, nullptr);
        }, this);
    }
}

void RainbowAnimation::OnLeftBinDataUpdate(const BinData& value, void* arg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    leftBinData_ = value;
}

void RainbowAnimation::OnRightBinDataUpdate(const BinData& value, void* arg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rightBinData_ = value;
}

RGB RainbowAnimation::HSVtoRGB(float h, float s, float v)
{
    float c = v * s;
    float x = c * (1 - std::fabs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;

    float r1, g1, b1;
    if (h < 60)      { r1 = c; g1 = x; b1 = 0; }
    else if (h < 120){ r1 = x; g1 = c; b1 = 0; }
    else if (h < 180){ r1 = 0; g1 = c; b1 = x; }
    else if (h < 240){ r1 = 0; g1 = x; b1 = c; }
    else if (h < 300){ r1 = x; g1 = 0; b1 = c; }
    else             { r1 = c; g1 = 0; b1 = x; }

    return {
        static_cast<uint8_t>((r1 + m) * 255),
        static_cast<uint8_t>((g1 + m) * 255),
        static_cast<uint8_t>((b1 + m) * 255)
    };
}

void RainbowAnimation::AnimateFrame()
{
    std::lock_guard<std::mutex> lock(mutex_);

    const int width = grid_.getWidth();
    const int height = grid_.getHeight();

    // Normalize amplitude to [0, 1]
    float normalized = std::clamp(leftBinData_.maxValue / 100.0f, 0.0f, 1.0f);

    // Get bright rainbow color
    RGB color = BinToRainbowRGB(leftBinData_.maxBin, leftBinData_.totalBins, normalized);

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
