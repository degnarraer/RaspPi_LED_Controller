#include <iostream>
#include <sstream>
#include "signal.h"
#include "i2s_microphone.h"
#include "fft_computer.h"
#include "websocket_server.h"
#include "deployment_manager.h"
#include "logger.h"
//#include "ws281x_led_controller.h"

std::shared_ptr<I2SMicrophone> mic = std::make_shared<I2SMicrophone>("snd_rpi_googlevoicehat_soundcar", "Microphone", 48000, 2, 1000, SND_PCM_FORMAT_S24_LE, SND_PCM_ACCESS_RW_INTERLEAVED, true, 200000);
std::shared_ptr<WebSocketServer> webSocketServer = std::make_shared<WebSocketServer>(3001);
std::shared_ptr<FFTComputer> fftComputer = std::make_shared<FFTComputer>("FFT Computer", "Microphone", "FFT Bands", 8192, 48000, (1 << 23) - 1, webSocketServer);
std::shared_ptr<spdlog::logger> logger_;
std::shared_ptr<DeploymentManager> deploymentManger = std::make_shared<DeploymentManager>();

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
    deploymentManger->clearFolderContentsWithSudo("/var/www/html");
    deploymentManger->copyFolderContentsWithSudo("/home/degnarraer/RaspPi_LED_Controller/front_end/dist", "/var/www/html");
    webSocketServer->Run();
    mic->StartReadingMicrophone();
    std::cin.get(); // Wait for user input to terminate the program

    return 0;
}