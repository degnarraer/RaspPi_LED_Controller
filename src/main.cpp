#include <iostream>
#include "signal.h"
#include "i2s_microphone.h"
#include <spdlog/spdlog.h>

void Microphone_Callback(const std::vector<int32_t>& data, const std::string& deviceName)
{
    spdlog::get("Microphone Logger")->trace("{} Callback", deviceName);
}

void InitializeLoggers() {
    spdlog::set_level(spdlog::level::info);
    if (!spdlog::get("Setup Logger")) {
        auto logger = spdlog::stdout_color_mt("Setup Logger");
        logger->set_level(spdlog::level::info);
        logger->info("Setup Logger Configured");
    }
    if (!spdlog::get("Microphone Logger")) {
        auto logger = spdlog::stdout_color_mt("Microphone Logger");
        logger->set_level(spdlog::level::trace);
        logger->info("Microphone Logger Configured");
    } 
}

int main() {
    InitializeLoggers();
    I2SMicrophone mic = I2SMicrophone("plughw:0,0", 44100, 2, 100);
    mic.ReadAudioData();
    mic.StartReading(Microphone_Callback);
    std::cin.get(); // Wait for user input to terminate the program
    return 0;
}
