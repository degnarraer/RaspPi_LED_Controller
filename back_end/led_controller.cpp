#include "led_controller.h"
#include "logger.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <cstring>
#include <chrono>

#define LED_CURRENT 20.0f // mA per LED, adjust as needed
#define BASE_LED_CURRENT 1.0f // mA per LED, adjust as needed

LED_Controller::LED_Controller(int ledCount)
    : ledCount_(ledCount)
    , logger_(initializeLogger("LED Logger", spdlog::level::info))
    , calculatedCurrentSignal_(std::dynamic_pointer_cast<Signal<float>>(SignalManager::getInstance().getSharedSignalByName("Calculated Current")))
    , currentLimitSignal_(std::dynamic_pointer_cast<Signal<uint32_t>>(SignalManager::getInstance().getSharedSignalByName("Current Limit")))
    , globalLedDriverLimitSignal_(std::dynamic_pointer_cast<Signal<uint8_t>>(SignalManager::getInstance().getSharedSignalByName("LED Driver Limit")))
    , running_(false)
    , render_in_progress_(false)
    , ledStrip_(ledCount)
{
    logger_->info("LED_Controller initialized with {} LEDs.", ledCount_);
    auto calculatedCurrentSignal = calculatedCurrentSignal_.lock();
    if(!calculatedCurrentSignal)
    {
        logger_->error("Failed to get current draw signal, it may not be initialized.");
    }
    else
    {
        logger_->info("Current draw signal initialized successfully.");
    }

    auto currentLimitSignal = currentLimitSignal_.lock();
    if(!currentLimitSignal)
    {
        logger_->error("Failed to get current draw signal, it may not be initialized.");
    }
    else
    {
        logger_->info("Current draw signal initialized successfully.");
        currentLimitSignal->setValue(currentLimit_);
    }

    auto globalLedDriverLimitSignal = globalLedDriverLimitSignal_.lock();
    if(!globalLedDriverLimitSignal)
    {
        logger_->error("Failed to get Global LED Driver Limit signal, it may not be initialized.");
    }
    else
    {
        logger_->info("Global LED Driver Limit signal initialized successfully.");
        globalLedDriverLimitSignal->setValue(globalLedDriverLimit_);
        globalLedDriverLimitSignal->registerSignalValueCallback([this](const std::uint8_t& value, void* arg) {
            std::lock_guard<std::mutex> lock(this->led_mutex_);
            this->logger_->info("LED Driver Limit Signal Callback: {}", value);
            this->globalLedDriverLimit_ = std::clamp<uint8_t>(value, 0, 31);
        }, this);
    }
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

    std::vector<uint8_t> frame;
    frame.reserve(4 + ledCount_ * 4 + (ledCount_ + 15) / 16);

    while (true)
    {
        float current_draw_mA = 0.0f;

        {
            std::lock_guard<std::mutex> lock(led_mutex_);
            if (!running_)
            {
                break;
            }

            render_in_progress_ = true;
            RenderGuard renderGuard(render_in_progress_);

            frame.clear();
            frame.insert(frame.end(), {0x00, 0x00, 0x00, 0x00}); // Start frame

            for (const auto& pixel : ledStrip_)
            {
                uint8_t ledControlByte = 0b11100000 | globalLedDriverLimit_;

                auto scaleColor = [](uint8_t color, float pixelBrightness, float globalBrightness) -> uint8_t {
                    float scaled = static_cast<float>(color) * pixelBrightness * globalBrightness;
                    return static_cast<uint8_t>(std::round(std::clamp(scaled, 0.0f, 255.0f)));
                };

                uint8_t r = scaleColor(pixel.color.r, pixel.brightness, globalUserBrightness_);
                uint8_t g = scaleColor(pixel.color.g, pixel.brightness, globalUserBrightness_);
                uint8_t b = scaleColor(pixel.color.b, pixel.brightness, globalUserBrightness_);

                frame.push_back(ledControlByte);
                frame.push_back(b);
                frame.push_back(g);
                frame.push_back(r);

                float intensity = (r + g + b) / (255.0f * 3.0f); // Normalize intensity to [0,1]
                float driver_limit_scalar = static_cast<float>(globalLedDriverLimit_) / 31.0f;
                float pixel_dynamic_mA = intensity * driver_limit_scalar * 3.0f * LED_CURRENT;
                current_draw_mA += pixel_dynamic_mA + BASE_LED_CURRENT;
            }

            int endFrameBytes = (ledCount_ + 15) / 16;
            frame.insert(frame.end(), endFrameBytes, 0xFF); // End frame
            sendLEDFrame(fd, frame);
        }

        // Logging and signal update outside lock
        logger_->debug("Estimated total current draw: {:.2f} mA", current_draw_mA);

        if (auto signal = calculatedCurrentSignal_.lock())
        {
            signal->setValue(current_draw_mA);
        }
        else
        {
            logger_->error("Current draw signal is not initialized, cannot update.");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    close(fd);
    logger_->info("LED render thread stopped.");
}

void LED_Controller::setColor(uint32_t color, float brightness)
{
    std::lock_guard<std::mutex> lock(led_mutex_);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    for (auto& pixel : ledStrip_)
    {
        pixel.color = {r, g, b};
        pixel.brightness = brightness;
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
    ledStrip_[index].brightness = std::clamp(brightness, 0.0f, 1.0f);
}

void LED_Controller::clear()
{
    std::lock_guard<std::mutex> lock(led_mutex_);
    for (auto& pixel : ledStrip_)
    {
        pixel.color = {0, 0, 0};
        pixel.brightness = 0.0f;
    }
    logger_->info("LEDs cleared.");
}

void LED_Controller::setUserGlobalBrightness(float brightness)
{
    std::lock_guard<std::mutex> lock(led_mutex_);

    globalUserBrightness_ = std::clamp(brightness, 0.0f, 1.0f);
    logger_->info("User global brightness set to {}", globalUserBrightness_);

    if (auto signal = globalUserBrightnessSignal_.lock())
    {
        signal->setValue(globalUserBrightness_);
    }
    else
    {
        logger_->warn("Failed to set user global brightness: signal is expired.");
    }
}


void LED_Controller::setGlobalLedDriverLimit(uint8_t limit)
{
    std::lock_guard<std::mutex> lock(led_mutex_);
    logger_->info("Device global brightness set to {}", globalLedDriverLimit_);
    globalLedDriverLimit_ = std::clamp<uint8_t>(limit, 0, 31);
    if (auto signal = globalLedDriverLimitSignal_.lock())
    {
        signal->setValue(globalLedDriverLimit_);
    }
    else
    {
        logger_->warn("Failed to set global LED driver limit: signal is expired.");
    }
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
    size_t totalWritten = 0;
    const uint8_t* buffer = data.data();
    size_t remaining = data.size();

    while (remaining > 0)
    {
        ssize_t written = write(fd, buffer + totalWritten, remaining);
        if (written < 0)
        {
            if (errno == EINTR)
            {
                continue; // Retry interrupted syscall
            }

            logger_->error("SPI write failed: {}", strerror(errno));
            return;
        }

        totalWritten += static_cast<size_t>(written);
        remaining -= static_cast<size_t>(written);
    }

    if (totalWritten != data.size())
    {
        logger_->error("SPI write incomplete: wrote {} of {} bytes", totalWritten, data.size());
    }
}

