#include <iostream>
#include "signal.h"
#include "i2s_microphone.h"
#include <spdlog/spdlog.h>

void Microphone_Callback(const std::vector<int32_t>& data)
{
    std::cout << "Callback!" << std::endl;
}

void InitializeLogger() {
    if (!spdlog::get("logger")) {
        auto console = spdlog::stdout_color_mt("logger");
        spdlog::set_level(spdlog::level::info);
    }
}

int main() {
    InitializeLogger();
    std::cout << "Hello, World!" << std::endl;
    I2SMicrophone mic = I2SMicrophone("Microphone", 44100, 2, 100);
    //mic.StartReading(Microphone_Callback);
    return 0;
}
