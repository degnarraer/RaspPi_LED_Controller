#include "led_controller.h"
#include "logger.h"

LED_Controller::LED_Controller(int ledCount, int gpioPin)
    : ledCount_(ledCount), gpioPin_(gpioPin)
{
    logger_ = initializeLogger("LED Logger", spdlog::level::info);
    rate_limited_log_ = std::make_shared<RateLimitedLogger>(logger_, std::chrono::milliseconds(10000));
    initializeLEDString();
}

LED_Controller::~LED_Controller()
{
    stop();
    ws2811_fini(&ledstring_);
    logger_->info("WS2811 cleaned up in destructor.");
}

void LED_Controller::initializeLEDString()
{
    ledstring_ = {
        .freq = WS2811_TARGET_FREQ,
        .dmanum = 10,
        .channel = {
            [0] = {0},
            [1] = {
                .gpionum = gpioPin_,
                .invert = 0,
                .count = ledCount_,
                .strip_type = WS2812_STRIP,
                .brightness = 255
            }
        }
    };
}

void LED_Controller::renderLoop()
{
    logger_->info("LED Render thread started.");
    while (running_)
    {
        if (render_in_progress_)
        {
            rate_limited_log_->log("render in progress", spdlog::level::warn, "LED render thread is busy. Waiting for completion.");
            while(render_in_progress_)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            continue;
        }
        render_in_progress_ = true;
        {
            std::lock_guard<std::mutex> lock(led_mutex_);
            ws2811_return_t ret = ws2811_render(&ledstring_);
            if (ret != WS2811_SUCCESS)
            {
                logger_->error("Render failed: {}", ws2811_get_return_t_str(ret));
            }
        }
        render_in_progress_ = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    logger_->info("LED render thread stopped.");
}

void LED_Controller::setColor(uint32_t color)
{
    std::lock_guard<std::mutex> lock(led_mutex_);
    for (int i = 0; i < ledCount_; ++i)
    {
        ledstring_.channel[1].leds[i] = color;
    }

    logger_->info("All LEDs set to color: #{:06X}", color & 0xFFFFFF);
}

void LED_Controller::run()
{
    logger_->info("Starting LED Renderer thread.");
    std::lock_guard<std::mutex> lock(led_mutex_);
    if (running_)
    {
        logger_->warn("LED_Controller is already running.");
        return;
    }

    ws2811_return_t ret = ws2811_init(&ledstring_);
    if (ret != WS2811_SUCCESS)
    {
        logger_->error("WS2811 initialization failed: {}", ws2811_get_return_t_str(ret));
        return;
    }
    logger_->info("WS2811 initialized successfully.");
    ledRenderThread_ = std::thread(&LED_Controller::renderLoop, this);
    running_ = true;
}

void LED_Controller::stop()
{
    running_ = false;
    if (ledRenderThread_.joinable())
    {
        ledRenderThread_.join();
    }

    std::lock_guard<std::mutex> lock(led_mutex_);
    ws2811_fini(&ledstring_);
    logger_->info("WS2811 cleaned up in stop().");
}

void LED_Controller::clear()
{
    std::lock_guard<std::mutex> lock(led_mutex_);
    for (int i = 0; i < ledCount_; i++)
    {
        ledstring_.channel[1].leds[i] = 0x000000;
    }

    ws2811_return_t ret = ws2811_render(&ledstring_);
    if (ret != WS2811_SUCCESS)
    {
        logger_->error("Render failed while clearing LEDs: {}", ws2811_get_return_t_str(ret));
    }
    else
    {
        logger_->info("LEDs cleared.");
    }
}


void LED_Controller::calculateCurrent()
{
    float current_draw_mA = 0.0f;

    for (int i = 0; i < ledCount_; i++)
    {
        uint32_t color = ledstring_.channel[1].leds[i];
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;

        current_draw_mA += ((r + g + b) / 255.0f) * 20.0f;
    }

    logger_->info("Estimated total current draw: {:.2f} mA", current_draw_mA);
}
