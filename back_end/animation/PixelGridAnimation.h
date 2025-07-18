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
    void stop();
    bool IsRunning() const;

protected:
    virtual void AnimateFrame() = 0;

    PixelGridSignal& grid_;
    int frameRate_;
    std::atomic<bool> running_;

private:
    void run();
    std::thread thread_;
};
