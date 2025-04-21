#include <iostream>
#include <sstream>
#include "signal.h"
#include "i2s_microphone.h"
#include "fft_computer.h"
#include "websocket_server.h"
#include "deployment_manager.h"
#include "logger.h"
#include "SystemStatusMonitor.h"
#include "SignalFactory.h"
//#include "ws281x_led_controller.h"

int main()
{
    std::shared_ptr<WebSocketServer> webSocketServer = std::make_shared<WebSocketServer>(8080);
    SignalFactory::CreateSignals(webSocketServer);

    std::shared_ptr<I2SMicrophone> mic = std::make_shared<I2SMicrophone>("snd_rpi_googlevoicehat_soundcar", "Microphone", 48000, 2, 1000, SND_PCM_FORMAT_S24_LE, SND_PCM_ACCESS_RW_INTERLEAVED, true, 200000, webSocketServer);
    std::shared_ptr<FFTComputer> fftComputer = std::make_shared<FFTComputer>("FFT Computer", "Microphone", "FFT Bands", 8192, 48000, (1 << 23) - 1, webSocketServer);
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<DeploymentManager> deploymentManger = std::make_shared<DeploymentManager>();
    std::shared_ptr<SystemStatusMonitor> systemStatusMonitor = std::make_shared<SystemStatusMonitor>(webSocketServer);

    logger_ = InitializeLogger("Main Logger", spdlog::level::info);
    deploymentManger->clearFolderContentsWithSudo("/var/www/html");
    deploymentManger->copyFolderContentsWithSudo("/home/degnarraer/RaspPi_LED_Controller/front_end/dist", "/var/www/html");
    webSocketServer->Run();
    mic->StartReadingMicrophone();
    systemStatusMonitor->startMonitoring();   
    std::cin.get();
    return 0;
}