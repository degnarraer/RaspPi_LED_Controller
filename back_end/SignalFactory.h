#pragma once

#include "signal.h"
#include "websocket_server.h"

class SignalFactory
{
public:
    static void CreateSignals(std::shared_ptr<WebSocketServer> webSocketServer)
    {
        // Ensure the WebSocketServer is provided
        if (!webSocketServer)
        {
            throw std::invalid_argument("WebSocketServer cannot be null");
        }

        SignalManager::GetInstance().CreateSignal<std::vector<int32_t>>("Microphone", webSocketServer);
        SignalManager::GetInstance().CreateSignal<std::vector<int32_t>>("Microphone Left Channel", webSocketServer);
        SignalManager::GetInstance().CreateSignal<std::vector<int32_t>>("Microphone Right Channel", webSocketServer);
        SignalManager::GetInstance().CreateSignal<std::vector<float>>("FFT Bands", webSocketServer, encode_FFT_Bands);
        SignalManager::GetInstance().CreateSignal<std::vector<float>>("FFT Bands Left Channel", webSocketServer, encode_FFT_Bands);
        SignalManager::GetInstance().CreateSignal<std::vector<float>>("FFT Bands Right Channel", webSocketServer, encode_FFT_Bands);
        SignalManager::GetInstance().CreateSignal<std::string>("CPU Usage", webSocketServer, encode_signal_name_and_value<std::string>);
        SignalManager::GetInstance().CreateSignal<std::string>("CPU Memory Usage", webSocketServer, encode_signal_name_and_value<std::string>);
        SignalManager::GetInstance().CreateSignal<std::string>("CPU Temp", webSocketServer, encode_signal_name_and_value<std::string>);
        SignalManager::GetInstance().CreateSignal<std::string>("GPU Temp", webSocketServer, encode_signal_name_and_value<std::string>);
        SignalManager::GetInstance().CreateSignal<std::string>("Throttle Status", webSocketServer, encode_signal_name_and_value<std::string>);
        SignalManager::GetInstance().CreateSignal<std::string>("Net RX", webSocketServer, encode_signal_name_and_value<std::string>);
        SignalManager::GetInstance().CreateSignal<std::string>("Net TX", webSocketServer, encode_signal_name_and_value<std::string>);
        SignalManager::GetInstance().CreateSignal<std::string>("Disk Usage", webSocketServer, encode_signal_name_and_value<std::string>);
        SignalManager::GetInstance().CreateSignal<std::string>("Load Avg", webSocketServer, encode_signal_name_and_value<std::string>);
        SignalManager::GetInstance().CreateSignal<std::string>("Uptime", webSocketServer, encode_signal_name_and_value<std::string>);
    }
};