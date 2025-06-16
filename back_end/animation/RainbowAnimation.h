#pragma once

#include "PixelGridAnimation.h"
#include "../signals/signal.h"
#include <vector>
#include <mutex>
#include "ColorFunctions.h"

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
    std::shared_ptr<Signal<ColorMappingType>> colorMappingTypeSignal_;
    BinData leftBinData_;
    BinData rightBinData_;
    ColorMappingType colorMappingType_ = ColorMappingType::Log2;
    std::mutex mutex_;
    std::shared_ptr<spdlog::logger> logger_;

};
