#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <memory>
#include <cstdint>
#include <spdlog/spdlog.h>
#include "signals/signal.h"

class LED_Controller
{
public:
    explicit LED_Controller( int ledCount );
    ~LED_Controller();

    void run();
    void stop();

    void setColor(uint32_t color);          // Set all LEDs color (full brightness)
    void setPixel(int index, uint8_t r, uint8_t g, uint8_t b, float brightness = 1.0f);
    void clear();

    // Set global user brightness (0.0 - 1.0)
    void setUserGlobalBrightness(float brightness);

    // Set global device brightness (0-31)
    void setDeviceGlobalBrightness(uint8_t brightness);

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

    float global_user_brightness_ = 1.0f;    // scales all pixels' colors/user brightness
    uint8_t global_device_brightness_ = 31;  // hardware brightness (0-31) applied to all LEDs

    std::shared_ptr<spdlog::logger> logger_;

    // 0.0 to 1.0, used to set the rendered brightness of each pixel
    std::weak_ptr<Signal<float>> brightnessSignal_;

    // 0.0 to some maximum current, used to limit total current draw
    std::weak_ptr<Signal<float>> currentLimitSignal_;
    
    // 1-31, used to limit driver draw
    std::weak_ptr<Signal<std::uint8_t>> driverLimitSignal_;

    // Constants
    static constexpr const char* SPI_DEVICE = "/dev/spidev0.0";
    static constexpr uint32_t SPI_SPEED_HZ = 20000000;
};
