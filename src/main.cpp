#include <iostream>
#include "signal.h"
#include "i2s_microphone.h"
#include "fft_computer.h"
#include "websocket_server.h"
#include "deployment_manager.h"
#include "logger.h"
#include <sstream>

WebSocketServer webSocketServer(8080);
std::shared_ptr<spdlog::logger> logger_;
DeploymentManager deploymentMananger;

void Microphone_Callback(const std::vector<int32_t>& data, const std::string& deviceName)
{
    logger_->debug("Device {}: Callback Called", deviceName);
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
    logger_->trace("Device {}: Callback Data: {}", deviceName, dataStr);
}

int main()
{
    logger_ = InitializeLogger("Main Logger", spdlog::level::info);
    deploymentMananger.clearFolderContentsWithSudo("/var/www/html");
    deploymentMananger.copyFolderContentsWithSudo("/home/degnarraer/RaspPi_LED_Controller/data", "/var/www/html");

    webSocketServer.Run();
    I2SMicrophone mic = I2SMicrophone("snd_rpi_googlevoicehat_soundcar", "Microphone", 48000, 2, 1000, SND_PCM_FORMAT_S24_LE, SND_PCM_ACCESS_RW_INTERLEAVED, true, 200000);
    FFTComputer fftComputer = FFTComputer("FFT Computer", "Microphone", 8192, 48000, (2^23)-1);
    mic.StartReadingMicrophone();
    //mic.StartReadingSineWave(1000);
    
    std::cin.get(); // Wait for user input to terminate the program

    return 0;
}