#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <memory>
#include <cstdint>
#include <spdlog/spdlog.h>
#include "signals/signal.h"

class RenderGuard
{
public:
    RenderGuard(bool& flag) : flag_(flag)
    {
        flag_ = true;
    }

    ~RenderGuard()
    {
        flag_ = false;
    }

private:
    bool& flag_;
};

class LED_Controller
{
public:
    explicit LED_Controller( int ledCount );
    ~LED_Controller();

    void run();
    void stop();

    void setColor(uint32_t color, float brightness);
    void setPixel(int index, uint8_t r, uint8_t g, uint8_t b, float brightness = 1.0f);
    void clear();

    // Set global user brightness (0.0 - 1.0)
    void setUserGlobalBrightness(float brightness);

    // Set global device brightness (0-31)
    void setGlobalLedDriverLimit(uint8_t limit);

private:
    void renderLoop();

    int openSPI();
    void sendLEDFrame(int fd, const std::vector<uint8_t>& data);

private:
    int ledCount_;
    std::vector<Pixel> ledStrip_;

    bool running_;
    bool render_in_progress_;
    std::thread ledRenderThread_;
    std::mutex led_mutex_;

    // 0.0 to 1.0 scales all pixels' colors/user brightness
    float globalUserBrightness_ = 1.0f;
    std::weak_ptr<Signal<float>> globalUserBrightnessSignal_;
    
    // hardware brightness (0-31) applied to all LEDs, used to limit driver current draw
    uint8_t globalLedDriverLimit_ = 1;
    std::weak_ptr<Signal<uint8_t>> globalLedDriverLimitSignal_;
    
    // used to limit total mA current draw
    uint32_t currentLimit_ = 2000;
    std::weak_ptr<Signal<uint32_t>> currentLimitSignal_;

    // Calculated current draw of the LEDs
    std::weak_ptr<Signal<float>> calculatedCurrentSignal_;

    std::shared_ptr<spdlog::logger> logger_;

    // Constants
    static constexpr const char* SPI_DEVICE = "/dev/spidev0.0";
    static constexpr uint32_t SPI_SPEED_HZ = 20000000;
};
