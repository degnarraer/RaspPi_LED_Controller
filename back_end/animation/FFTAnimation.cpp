#include "FFTAnimation.h"

FFTAnimation::FFTAnimation(PixelGridSignal& grid)
    : PixelGridAnimation(grid, 30)
{
    auto leftBase = SignalManager::GetInstance().GetSharedSignalByName("FFT Bands Left Channel");
    auto rightBase = SignalManager::GetInstance().GetSharedSignalByName("FFT Bands Right Channel");

    fftLeft_ = std::dynamic_pointer_cast<Signal<std::vector<float>>>(leftBase);
    fftRight_ = std::dynamic_pointer_cast<Signal<std::vector<float>>>(rightBase);

    if (fftLeft_)
    {
        fftLeft_->RegisterCallback([this](const std::vector<float>& value, void*) {
            OnLeftUpdate(value, nullptr);
        }, this);
    }

    if (fftRight_)
    {
        fftRight_->RegisterCallback([this](const std::vector<float>& value, void*) {
            OnRightUpdate(value, nullptr);
        }, this);
    }
}

void FFTAnimation::OnLeftUpdate(const std::vector<float>& value, void*)
{
    std::lock_guard<std::mutex> lock(mutex_);
    leftBands_ = value;
}

void FFTAnimation::OnRightUpdate(const std::vector<float>& value, void*)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rightBands_ = value;
}

void FFTAnimation::AnimateFrame()
{
    std::lock_guard<std::mutex> lock(mutex_);

    const int width = grid_.GetWidth();
    const int height = grid_.GetHeight();
    const int halfWidth = width / 2;

    grid_.Clear();

    for (int x = 0; x < std::min<int>(leftBands_.size(), halfWidth); ++x)
    {
        int barHeight = static_cast<int>(leftBands_[x] * height);
        for (int y = 0; y < std::min(barHeight, height); ++y)
        {
            grid_.SetPixel(x, height - 1 - y, {255, 0, 0});
        }
    }

    for (int x = 0; x < std::min<int>(rightBands_.size(), halfWidth); ++x)
    {
        int barHeight = static_cast<int>(rightBands_[x] * height);
        for (int y = 0; y < std::min(barHeight, height); ++y)
        {
            grid_.SetPixel(halfWidth + x, height - 1 - y, {0, 0, 255});
        }
    }

    grid_.Notify();

}
