#include <iostream>
#include <sstream>
#include "i2s_microphone.h"
#include "fft_computer.h"
#include "websocket_server.h"
#include "deployment_manager.h"
#include "logger.h"
#include "SystemStatusMonitor.h"
#include "signals/SignalFactory.h"
#include "signals/signal.h"
#include "signals/PixelGridSignal.h"
#include "./animation/FFTAnimation.h"
#include "./animation/RainbowAnimation.h"
#include "RPIConfigEditor.h"

int main()
{
    std::shared_ptr<spdlog::logger> logger_;
    logger_ = initializeLogger("Main Logger", spdlog::level::info);
    
    auto configEditor = std::make_shared<RPiConfigEditor>();
    std::vector<std::string> requiredDtparams = {
        "dtparam=i2s=on",
        "dtparam=spi=on",
        "dtparam=watchdog=on"
    };
    std::vector<std::string> requiredDtoverlays = {
        "dtoverlay=googlevoicehat-soundcard"
    };

    configEditor->ensureParametersEnabled(requiredDtparams, requiredDtoverlays);

    auto webSocketServer = std::make_shared<WebSocketServer>(8080);
    SignalFactory::CreateSignals(webSocketServer);
    auto mic = std::make_shared<I2SMicrophone>("snd_rpi_googlevoicehat_soundcar", "Microphone", 48000, 1024, SND_PCM_FORMAT_S24_LE, SND_PCM_ACCESS_RW_INTERLEAVED, true, 200000, webSocketServer);
    auto fftComputer = std::make_shared<FFTComputer>("FFT Computer", "Microphone", "FFT Computer", 8192, 48000, (1 << 23) - 1, webSocketServer);
    auto deploymentManger = std::make_shared<DeploymentManager>();
    auto systemStatusMonitor = std::make_shared<SystemStatusMonitor>(webSocketServer);
    
    deploymentManger->clearFolderContentsWithSudo("/var/www/html");
    deploymentManger->copyFolderContentsWithSudo("./www", "/var/www/html");
    webSocketServer->start();
    mic->startReadingMicrophone();
    systemStatusMonitor->startMonitoring();
    PixelGridSignal grid("Pixel Grid", 5, 144, webSocketServer);
    RainbowAnimation animation(grid);
    animation.Start();

    std::cin.get();

    animation.stop();
    webSocketServer->stop();
    return 0;
}