#pragma once

#include <memory>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <ws2811.h>

class LED_Controller
{
public:
    LED_Controller(int ledCount = 60, int gpioPin = 13);
    ~LED_Controller();

    void Run();
    void Clear();
    void CalculateCurrent();

private:
    std::shared_ptr<spdlog::logger> logger_;
    ws2811_t ledstring_;
    int ledCount_;
    int gpioPin_;

    void InitializeLEDString();
};
