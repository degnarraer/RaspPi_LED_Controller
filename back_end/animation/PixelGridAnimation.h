#pragma once

#include "PixelGridSignal.h"
#include <thread>
#include <atomic>
#include <chrono>

class PixelGridAnimation
{
public:
    explicit PixelGridAnimation(PixelGridSignal& grid, int frameRate = 100);
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
