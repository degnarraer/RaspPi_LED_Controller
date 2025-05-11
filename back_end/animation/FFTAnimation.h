#pragma once

#include "PixelGridAnimation.h"
#include "../signals/signal.h"
#include <vector>
#include <mutex>

class FFTAnimation : public PixelGridAnimation
{
public:
    explicit FFTAnimation(PixelGridSignal& grid);
    ~FFTAnimation() override = default;

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
};
