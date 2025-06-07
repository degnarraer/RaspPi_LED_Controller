#include "led_controller.h"
#include "logger.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <cstring>
#include <chrono>

LED_Controller::LED_Controller(int ledCount)
    : ledCount_(ledCount)
    , running_(false)
    , render_in_progress_(false)
    , ledStrip_(ledCount)
{
    logger_ = initializeLogger("LED Logger", spdlog::level::info);
    logger_->info("LED_Controller initialized with {} LEDs.", ledCount_);
}

LED_Controller::~LED_Controller()
{
    stop();
    logger_->info("LED_Controller cleaned up in destructor.");
}

void LED_Controller::run()
{
    std::lock_guard<std::mutex> lock(led_mutex_);
    if (running_)
    {
        logger_->warn("LED_Controller is already running.");
        return;
    }

    running_ = true;
    ledRenderThread_ = std::thread(&LED_Controller::renderLoop, this);
    logger_->info("LED Renderer thread started.");
}

void LED_Controller::stop()
{
    {
        std::lock_guard<std::mutex> lock(led_mutex_);
        running_ = false;
    }
    if (ledRenderThread_.joinable())
    {
        ledRenderThread_.join();
    }
    logger_->info("LED_Controller stopped.");
}

void LED_Controller::renderLoop()
{
    int fd = openSPI();
    if (fd < 0)
    {
        logger_->error("Failed to open SPI device.");
        return;
    }

    while (true)
    {
        {
            std::lock_guard<std::mutex> lock(led_mutex_);
            if (!running_)
            {
                break;
            }
            render_in_progress_ = true;

            std::vector<uint8_t> frame;
            frame.insert(frame.end(), {0x00, 0x00, 0x00, 0x00}); // Start frame

            for (const auto& pixel : ledStrip_)
            {
                // Combine hardware brightness with APA102 format
                uint8_t hw_brightness = (pixel.device_brightness * global_device_brightness_) / 31;
                hw_brightness &= 0x1F;
                uint8_t br = 0b11100000 | hw_brightness;

                // Color scaled by user animation brightness and global brightness
                uint8_t r = static_cast<uint8_t>(pixel.color.r * pixel.brightness * global_user_brightness_);
                uint8_t g = static_cast<uint8_t>(pixel.color.g * pixel.brightness * global_user_brightness_);
                uint8_t b = static_cast<uint8_t>(pixel.color.b * pixel.brightness * global_user_brightness_);

                frame.push_back(br);
                frame.push_back(b);
                frame.push_back(g);
                frame.push_back(r);
            }

            int endFrameBytes = (ledCount_ + 15) / 16;
            frame.insert(frame.end(), endFrameBytes, 0xFF); // End frame

            sendLEDFrame(fd, frame);

            render_in_progress_ = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    close(fd);
    logger_->info("LED render thread stopped.");
}

void LED_Controller::setColor(uint32_t color)
{
    std::lock_guard<std::mutex> lock(led_mutex_);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    for (auto& pixel : ledStrip_)
    {
        pixel.color = {r, g, b};
        pixel.brightness = 1.0f;       // Full animation brightness by default
    }
    logger_->info("All LEDs set to color: #{:06X}", color & 0xFFFFFF);
}

void LED_Controller::setPixel(int index, uint8_t r, uint8_t g, uint8_t b, float brightness)
{
    std::lock_guard<std::mutex> lock(led_mutex_);
    if (index < 0 || index >= ledCount_)
    {
        logger_->warn("setPixel index {} out of bounds", index);
        return;
    }

    ledStrip_[index].color = {r, g, b};
    ledStrip_[index].brightness = brightness;
    // device_brightness left unchanged here; controlled by current limiter
}

void LED_Controller::clear()
{
    std::lock_guard<std::mutex> lock(led_mutex_);
    for (auto& pixel : ledStrip_)
    {
        pixel.color = {0, 0, 0};
        pixel.brightness = 0.0f;
        pixel.device_brightness = 31;
    }
    logger_->info("LEDs cleared.");
}

void LED_Controller::calculateCurrent()
{
    float current_draw_mA = 0.0f;
    std::lock_guard<std::mutex> lock(led_mutex_);
    for (const auto& px : ledStrip_)
    {
        // Simple estimation: total color intensity scaled by animation brightness times 20mA max per LED
        float intensity = (px.color.r + px.color.g + px.color.b) / 255.0f;
        current_draw_mA += intensity * px.brightness * 20.0f;
    }
    logger_->info("Estimated total current draw: {:.2f} mA", current_draw_mA);
}

void LED_Controller::setUserGlobalBrightness(float brightness)
{
    std::lock_guard<std::mutex> lock(led_mutex_);
    if (brightness < 0.0f) brightness = 0.0f;
    if (brightness > 1.0f) brightness = 1.0f;
    global_user_brightness_ = brightness;
    logger_->info("User global brightness set to {:.2f}", global_user_brightness_);
}

void LED_Controller::setDeviceGlobalBrightness(uint8_t brightness)
{
    std::lock_guard<std::mutex> lock(led_mutex_);
    if (brightness > 31) brightness = 31;
    global_device_brightness_ = brightness;
    logger_->info("Device global brightness set to {}", global_device_brightness_);
}

int LED_Controller::openSPI()
{
    int fd = open(SPI_DEVICE, O_WRONLY);
    if (fd < 0)
    {
        perror("SPI open failed");
        return -1;
    }

    uint8_t mode = SPI_MODE_0;
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0)
    {
        perror("SPI set mode failed");
        close(fd);
        return -1;
    }

    uint32_t speed = SPI_SPEED_HZ;
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
    {
        perror("SPI set speed failed");
        close(fd);
        return -1;
    }

    return fd;
}

void LED_Controller::sendLEDFrame(int fd, const std::vector<uint8_t>& data)
{
    ssize_t written = write(fd, data.data(), data.size());
    if (written != static_cast<ssize_t>(data.size()))
    {
        logger_->error("SPI write incomplete: wrote {} of {} bytes", written, data.size());
    }
}
