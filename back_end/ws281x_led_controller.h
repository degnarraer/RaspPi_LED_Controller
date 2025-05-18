#include <ws2811.h>

// Number of LEDs
#define LED_COUNT 50
#define GPIO_PIN 18  // GPIO pin for data line

// Create the ws2811_t structure to hold LED configuration
ws2811_t ledstring = {
    .freq = WS2811_TARGET_FREQ,
    .dmanum = 10,
    .channel = {
        [0] = {
            .gpionum = GPIO_PIN,
            .invert = 0,
            .count = LED_COUNT,
            .strip_type = WS2811_STRIP_GRB,
            .leds = NULL,
            .brightness = 255,
            .wshift = 0,
            .rshift = 0,
            .gshift = 0,
            .bshift = 0,
            .gamma = NULL
        },
        [1] = {0}
    }
};

void run_leds()
{
    if (ws2811_init(&ledstring)) {
        std::cerr << "WS2811 Initialization failed!" << std::endl;
        return;
    }

    // Set a color on the first LED (example: red)
    ledstring.channel[0].leds[0] = 0xFF0000;  // Red color

    // Render the LEDs to update the color
    ws2811_render(&ledstring);

    // Clean up and reset the LEDs before exiting
    ws2811_fini(&ledstring);
}


void clear_leds()
{
    // Clear the LEDs
    for (int i = 0; i < LED_COUNT; i++) {
        ledstring.channel[0].leds[i] = 0x000000;  // Turn off the LED
    }
    ws2811_render(&ledstring);
}

void calculate_current()
{
    std::cout << "Calculating current draw..." << std::endl;
    float current_draw_mA = 0.0f;
    for (int i = 0; i < LED_COUNT; i++) {
        uint32_t color = ledstring.channel[0].leds[i];
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;

        // Estimate: each color channel at full brightness draws ~20mA
        current_draw_mA += (r + g + b) / 255.0f * 20.0f;
    }
    std::cout << "Estimated total current draw: " << current_draw_mA << " mA" << std::endl;
}
