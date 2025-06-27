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
    BinData leftBinData_;
    std::weak_ptr<Signal<BinData>> leftBinDataSignal_;
    
    BinData rightBinData_;
    std::weak_ptr<Signal<BinData>> rightBinDataSignal_;
    
    ColorMappingType colorMappingType_ = ColorMappingType::Linear;
    std::weak_ptr<Signal<std::string>> colorMappingTypeSignal_;

    std::mutex mutex_;
    std::shared_ptr<spdlog::logger> logger_;

};
