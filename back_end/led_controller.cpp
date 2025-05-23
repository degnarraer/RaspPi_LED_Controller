#include "led_controller.h"
#include "logger.h"

LED_Controller::LED_Controller(int ledCount, int gpioPin)
    : ledCount_(ledCount), gpioPin_(gpioPin)
{
    logger_ = InitializeLogger("LED Logger", spdlog::level::info);
    InitializeLEDString();
}

LED_Controller::~LED_Controller()
{
    ws2811_fini(&ledstring_);
    logger_->info("WS2811 cleaned up in destructor.");
}

void LED_Controller::InitializeLEDString()
{
    ledstring_ = {
        .freq = WS2811_TARGET_FREQ,
        .dmanum = 10,
        .channel = {
            [0] = {
                .gpionum = gpioPin_,
                .invert = 0,
                .count = ledCount_,
                .strip_type = WS2811_STRIP_RGB,
                .brightness = 255
            },
            [1] = {0}
        }
    };
}

void LED_Controller::Run()
{
    ws2811_return_t ret = ws2811_init(&ledstring_);
    if (ret != WS2811_SUCCESS)
    {
        logger_->error("WS2811 initialization failed: {}", ws2811_get_return_t_str(ret));
        return;
    }

    logger_->info("WS2811 initialized successfully.");

    for(int i = 0; i < ledCount_; i++)
    {
        ledstring_.channel[0].leds[i] = 0xFF0000;
    }

    ret = ws2811_render(&ledstring_);
    if (ret != WS2811_SUCCESS)
    {
        logger_->error("Render failed: {}", ws2811_get_return_t_str(ret));
    }
    else
    {
        logger_->info("LED strip rendered to red color.");
    }
}

void LED_Controller::Clear()
{
    for (int i = 0; i < ledCount_; i++)
    {
        ledstring_.channel[0].leds[i] = 0x000000;
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

void LED_Controller::CalculateCurrent()
{
    float current_draw_mA = 0.0f;

    for (int i = 0; i < ledCount_; i++)
    {
        uint32_t color = ledstring_.channel[0].leds[i];
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;

        current_draw_mA += ((r + g + b) / 255.0f) * 20.0f;
    }

    logger_->info("Estimated total current draw: {:.2f} mA", current_draw_mA);
}
