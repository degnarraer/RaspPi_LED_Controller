#include "FFTAnimation.h"

FFTAnimation::FFTAnimation(PixelGridSignal& grid)
    : PixelGridAnimation(grid, 30)
{
    auto leftBase = SignalManager::getInstance().getSharedSignalByName("FFT Computer Left Channel");
    auto rightBase = SignalManager::getInstance().getSharedSignalByName("FFT Computer Right Channel");

    fftLeft_ = std::dynamic_pointer_cast<Signal<std::vector<float>>>(leftBase);
    fftRight_ = std::dynamic_pointer_cast<Signal<std::vector<float>>>(rightBase);

    if (fftLeft_)
    {
        fftLeft_->registerSignalValueCallback([this](const std::vector<float>& value, void*) {
            OnLeftUpdate(value, nullptr);
        }, this);
    }

    if (fftRight_)
    {
        fftRight_->registerSignalValueCallback([this](const std::vector<float>& value, void*) {
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

    const int width = grid_.getWidth();
    const int height = grid_.getHeight();
    const int halfWidth = width / 2;

    grid_.clear();

    for (int x = 0; x < leftBands_.size(); ++x)
    {
        float normalized = std::clamp(leftBands_[x] / 10.0f, 0.0f, 1.0f);
        int barHeight = static_cast<int>(normalized * height);
    
        for (int y = 0; y < std::min(barHeight, height); ++y)
        {
            grid_.setPixel(x, height - 1 - y, {255, 0, 0});
        }
    }

    grid_.notify();

}
