#pragma once

#include <memory>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <ws2811.h>
#include "logger.h"

class LED_Controller
{
public:
    LED_Controller(int ledCount = 60, int gpioPin = 13);
    ~LED_Controller();

    void Run();
    void Stop();
    void Clear();
    void SetColor(uint32_t color);
    void CalculateCurrent();

private:
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<RateLimitedLogger> rate_limited_log_;
    std::thread ledRenderThread_;
    ws2811_t ledstring_;
    std::mutex led_mutex_;
    std::atomic<bool> render_in_progress_ = false;
    int ledCount_;
    int gpioPin_;
    bool running_;

    void InitializeLEDString();
    void RenderLoop();
};
