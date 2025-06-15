#pragma once

#include "PixelGridAnimation.h"
#include "../signals/signal.h"
#include <vector>
#include <mutex>

class RainbowAnimation : public PixelGridAnimation
{
public:
    explicit RainbowAnimation(PixelGridSignal& grid);
    ~RainbowAnimation() override = default;

protected:
    void AnimateFrame() override;

private:
    std::shared_ptr<Signal<BinData>> rightBinDataSignal_;
    std::shared_ptr<Signal<BinData>> leftBinDataSignal_;
    BinData leftBinData_;
    BinData rightBinData_;
    std::mutex mutex_;

    void OnLeftBinDataUpdate(const BinData& value, void* arg);
    void OnRightBinDataUpdate(const BinData& value, void* arg);
    RGB HSVtoRGB(float h, float s, float v);
    RGB BinToRainbowRGB(int bin, int totalBins, float amplitudeNormalized)
    {
        // Avoid log(0)
        float binLog = std::log2(static_cast<float>(bin + 1));
        float totalLog = std::log2(static_cast<float>(totalBins));

        float hue = (binLog / totalLog) * 360.0f;

        return HSVtoRGB(hue, 1.0f, amplitudeNormalized);
    }
};
