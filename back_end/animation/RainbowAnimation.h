#pragma once

#include "PixelGridAnimation.h"
#include "../signal.h"
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
    std::shared_ptr<Signal<std::vector<float>>> fftLeft_;
    std::shared_ptr<Signal<std::vector<float>>> fftRight_;

    std::vector<float> leftBands_;
    std::vector<float> rightBands_;
    std::mutex mutex_;

    void OnLeftUpdate(const std::vector<float>& value, void* arg);
    void OnRightUpdate(const std::vector<float>& value, void* arg);
    RGB HSVtoRGB(float h, float s, float v);
    RGB BinToRainbowRGB(int bin, float amplitudeNormalized)
    {
        float hue = (static_cast<float>(bin) / 31.0f) * 360.0f;
        return HSVtoRGB(hue, 1.0f, amplitudeNormalized);
    }
};
