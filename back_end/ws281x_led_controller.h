#include <ws2811.h>

/ Number of LEDs
#define LED_COUNT 50
#define GPIO_PIN 18  // GPIO pin for data line

// Create the ws2811_t structure to hold LED configuration
ws2811_t ledstring = {
    .freq = WS2811_TARGET_FREQ,  // LED signal frequency
    .dma = 10,                   // DMA channel (use 10 by default)
    .channel = {
        [0] = {
            .gpionum = GPIO_PIN,  // GPIO pin for LEDs
            .count = LED_COUNT,    // Number of LEDs in this channel
            .invert = false,       // Don't invert the signal
            .brightness = 255,     // Maximum brightness
            .strip_type = WS2811_STRIP_GRB, // Color order (GRB for WS2812)
        }
    }
};

void run_leds()
{
    if (ws2811_init(&ledstring)) {
        std::cerr << "WS2811 Initialization failed!" << std::endl;
        return -1;
    }

    // Set a color on the first LED (example: red)
    ledstring.channel[0].leds[0] = 0xFF0000;  // Red color

    // Render the LEDs to update the color
    ws2811_render(&ledstring);

    // Clean up and reset the LEDs before exiting
    ws2811_fini(&ledstring);
}