#pragma once

#include "signal.h"
#include "IntVectorSignal.h"
#include "../websocket_server.h"

class SignalFactory
{
public:
    static void CreateSignals(std::shared_ptr<WebSocketServer> webSocketServer)
    {
        if (!webSocketServer)
        {
            throw std::invalid_argument("WebSocketServer cannot be null");
        }
        IntVectorSignal("Microphone", webSocketServer);
        IntVectorSignal("Microphone Left Channel", webSocketServer);
        IntVectorSignal("Microphone Right Channel", webSocketServer);
        SignalManager& signalManager = SignalManager::getInstance();
        signalManager.createSignal<std::vector<float>>("FFT Bands", webSocketServer, encode_FFT_Bands);
        signalManager.createSignal<std::vector<float>>("FFT Bands Left Channel", webSocketServer, encode_FFT_Bands);
        signalManager.createSignal<std::vector<float>>("FFT Bands Right Channel", webSocketServer, encode_FFT_Bands);
        signalManager.createSignal<std::string>("CPU Usage", webSocketServer, get_signal_and_value_encoder<std::string>());
        signalManager.createSignal<std::string>("CPU Memory Usage", webSocketServer, get_signal_and_value_encoder<std::string>());
        signalManager.createSignal<std::string>("CPU Temp", webSocketServer, get_signal_and_value_encoder<std::string>());
        signalManager.createSignal<std::string>("GPU Temp", webSocketServer, get_signal_and_value_encoder<std::string>());
        signalManager.createSignal<std::string>("Throttle Status", webSocketServer, get_signal_and_value_encoder<std::string>());
        signalManager.createSignal<std::string>("Net RX", webSocketServer, get_signal_and_value_encoder<std::string>());
        signalManager.createSignal<std::string>("Net TX", webSocketServer, get_signal_and_value_encoder<std::string>());
        signalManager.createSignal<std::string>("Disk Usage", webSocketServer, get_signal_and_value_encoder<std::string>());
        signalManager.createSignal<std::string>("Load Avg", webSocketServer, get_signal_and_value_encoder<std::string>());
        signalManager.createSignal<std::string>("Uptime", webSocketServer, get_signal_and_value_encoder<std::string>());

        signalManager.createSignal<std::string>("Min Microphone Limit", webSocketServer, get_signal_and_value_encoder<std::string>());
        signalManager.createSignal<std::string>("Max Microphone Limit", webSocketServer, get_signal_and_value_encoder<std::string>());
    }
};