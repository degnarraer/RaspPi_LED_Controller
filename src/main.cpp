#include <iostream>
#include "signal.h"
#include "i2s_microphone.h"
#include <spdlog/spdlog.h>

void Microphone_Callback(const std::vector<int32_t>& data)
{
    std::cout << "Callback!" << std::endl;
}

void InitializeLoggers() {
    spdlog::set_level(spdlog::level::info);
    if (!spdlog::get("Setup Logger")) {
        auto logger = spdlog::stdout_color_mt("Setup Logger");
        logger->set_level(spdlog::level::err);
        logger->info("Setup Logger Configured");
    }
    if (!spdlog::get("Microphone Logger")) {
        auto logger = spdlog::stdout_color_mt("Microphone Logger");
        logger->set_level(spdlog::level::err);
        logger->info("Microphone Logger Configured");
    } 
}

int main() {
    InitializeLoggers();
    spdlog::get("Setup Logger")->info("Hello World!");
    I2SMicrophone mic = I2SMicrophone("hw:1,0", 44100, 2, 100);
    //mic.StartReading(Microphone_Callback);
    return 0;
}
