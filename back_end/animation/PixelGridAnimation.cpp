#include "PixelGridAnimation.h"

PixelGridAnimation::PixelGridAnimation(PixelGridSignal& grid, int frameRate)
    : grid_(grid), frameRate_(frameRate), running_(false)
{}

PixelGridAnimation::~PixelGridAnimation()
{
    Stop();
}

void PixelGridAnimation::Start()
{
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&PixelGridAnimation::Run, this);
}

void PixelGridAnimation::Stop()
{
    if (!running_) return;
    running_ = false;
    if (thread_.joinable())
    {
        thread_.join();
    }
}

bool PixelGridAnimation::IsRunning() const
{
    return running_;
}

void PixelGridAnimation::Run()
{
    using namespace std::chrono;
    const auto frameTime = milliseconds(1000 / frameRate_);

    while (running_)
    {
        auto start = steady_clock::now();
        AnimateFrame();
        auto duration = steady_clock::now() - start;
        if (duration < frameTime)
        {
            std::this_thread::sleep_for(frameTime - duration);
        }
    }
}
