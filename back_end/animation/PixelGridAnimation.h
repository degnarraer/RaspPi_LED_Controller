#pragma once

#include "../signals/PixelGridSignal.h"
#include <thread>
#include <atomic>
#include <chrono>

class PixelGridAnimation
{
public:
    explicit PixelGridAnimation(PixelGridSignal& grid, int frameRate = 1);
    virtual ~PixelGridAnimation();

    void Start();
    void Stop();
    bool IsRunning() const;

protected:
    virtual void AnimateFrame() = 0;

    PixelGridSignal& grid_;
    int frameRate_;
    std::atomic<bool> running_;

private:
    void Run();
    std::thread thread_;
};
