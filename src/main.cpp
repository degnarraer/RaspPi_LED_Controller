#include <iostream>
#include "signal.h"
#include "i2s_microphone.h"
#include "fft_computer.h"
#include <spdlog/spdlog.h>
#include <sstream>

void Microphone_Callback(const std::vector<int32_t>& data, const std::string& deviceName)
{
    spdlog::get("Microphone Logger")->debug("Device {}: Callback Called", deviceName);
    // Convert the data vector to a string
    std::ostringstream oss;
    for (size_t i = 0; i < data.size(); ++i) {
        oss << data[i];
        if (i != data.size() - 1) {
            oss << ", "; // Add comma separator between values
        }
    }
    std::string dataStr = oss.str(); // The entire data as a string

    // Log the data as a trace message
    spdlog::get("Microphone Logger")->trace("Device {}: Callback Data: {}", deviceName, dataStr);
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
        logger->set_level(spdlog::level::info);
        logger->info("Microphone Logger Configured");
    } 
}

int main() {
    InitializeLoggers();
    I2SMicrophone mic = I2SMicrophone("plughw:0,0", 44100, 2, 100);
    mic.ReadAudioData();
    mic.RegisterCallback(Microphone_Callback);
    mic.StartReading();
    std::cin.get(); // Wait for user input to terminate the program
    return 0;
}
