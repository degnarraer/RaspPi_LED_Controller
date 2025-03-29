#include <iostream>
#include "signal.h"
#include "i2s_microphone.h"
#include "fft_computer.h"
#include "websocket_server.h"
#include "deployment_manager.h"
#include <spdlog/spdlog.h>
#include <sstream>

void Microphone_Callback(const std::vector<int32_t>& data, const std::string& deviceName)
{
    spdlog::get("Main Logger")->debug("Device {}: Callback Called", deviceName);
    // Convert the data vector to a string
    std::ostringstream oss;
    for (size_t i = 0; i < data.size(); ++i)
    {
        oss << data[i];
        if (i != data.size() - 1)
        {
            oss << ", "; // Add comma separator between values
        }
    }
    std::string dataStr = oss.str(); // The entire data as a string

    // Log the data as a trace message
    spdlog::get("Main Logger")->trace("Device {}: Callback Data: {}", deviceName, dataStr);
}

void LaunchWebSocketServer()
{
    boost::asio::io_context ioc;
    WebSocketServer server(ioc, 8080);
}

void InitializeLogger(const std::string loggerName, spdlog::level::level_enum level)
{
    // Retrieve existing logger or create a new one
    if (!spdlog::get(loggerName))
    {
        auto logger = spdlog::stdout_color_mt(loggerName);
        logger->set_level(level);
        logger->info("{} Configured", loggerName);
    } 
}

void InitializeLoggers()
{
    spdlog::set_level(spdlog::level::info);
    InitializeLogger("Main Logger", spdlog::level::info);
    InitializeLogger("Deployment Manager", spdlog::level::info);
    InitializeLogger("Signal Logger", spdlog::level::info);
    InitializeLogger("Microphone Logger", spdlog::level::info);
    InitializeLogger("FFT Computer Logger", spdlog::level::info);
    InitializeLogger("Web Socket Server Logger", spdlog::level::info);
    InitializeLogger("Web Socket Session Logger", spdlog::level::info);
}


int main()
{
    InitializeLoggers();
    DeploymentManager().clearFolderContentsWithSudo("/var/www/html");
    DeploymentManager().copyFolderContentsWithSudo("/home/degnarraer/RaspPi_LED_Controller/data", "/var/www/html");
    //
    LaunchWebSocketServer();
    I2SMicrophone mic = I2SMicrophone("snd_rpi_googlevoicehat_soundcar", "Microphone", 48000, 2, 1000, SND_PCM_FORMAT_S24_LE, SND_PCM_ACCESS_RW_INTERLEAVED, true, 200000);
    FFTComputer fftComputer = FFTComputer("FFT Computer", "Microphone", 8192, 48000, (2^23)-1);
    mic.StartReadingMicrophone();
    //mic.StartReadingSineWave(1000);
    
    std::cin.get(); // Wait for user input to terminate the program

    return 0;
}