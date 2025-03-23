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

void InitializeLogger(const std::string loggerName, spdlog::level::level_enum level){
    if (!spdlog::get(loggerName)) {
        auto logger = spdlog::stdout_color_mt(loggerName);
        logger->set_level(level);
        logger->info("{} Configured", loggerName);
    } 
}

void InitializeLoggers() {
    spdlog::set_level(spdlog::level::info);
    InitializeLogger("Signal Logger", spdlog::level::err);
    InitializeLogger("Setup Logger", spdlog::level::err);
    InitializeLogger("Microphone Logger", spdlog::level::err);
    InitializeLogger("FFT Computer Logger", spdlog::level::trace);
}

int main() {
    InitializeLoggers();
    I2SMicrophone mic = I2SMicrophone("plughw:0,0", "Microphone", 44100, 2, 1000, SND_PCM_FORMAT_S32_LE, SND_PCM_ACCESS_RW_INTERLEAVED);
    FFTComputer fftComputer = FFTComputer("FFT Computer", "Microphone", 8192, 44100);
    mic.ReadAudioData();
    mic.StartReading();
    std::cin.get(); // Wait for user input to terminate the program
    return 0;
}
