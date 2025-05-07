#pragma once

#include "signals/signal.h"
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

        SignalManager::GetInstance().CreateSignal<std::vector<int32_t>>("Microphone");
        SignalManager::GetInstance().CreateSignal<std::vector<int32_t>>("Microphone Left Channel");
        SignalManager::GetInstance().CreateSignal<std::vector<int32_t>>("Microphone Right Channel");
        SignalManager::GetInstance().CreateSignal<std::vector<float>>("FFT Bands", webSocketServer, encode_FFT_Bands);
        SignalManager::GetInstance().CreateSignal<std::vector<float>>("FFT Bands Left Channel", webSocketServer, encode_FFT_Bands);
        SignalManager::GetInstance().CreateSignal<std::vector<float>>("FFT Bands Right Channel", webSocketServer, encode_FFT_Bands);
        SignalManager::GetInstance().CreateSignal<std::string>("CPU Usage", webSocketServer, get_signal_and_value_encoder<std::string>());
        SignalManager::GetInstance().CreateSignal<std::string>("CPU Memory Usage", webSocketServer, get_signal_and_value_encoder<std::string>());
        SignalManager::GetInstance().CreateSignal<std::string>("CPU Temp", webSocketServer, get_signal_and_value_encoder<std::string>());
        SignalManager::GetInstance().CreateSignal<std::string>("GPU Temp", webSocketServer, get_signal_and_value_encoder<std::string>());
        SignalManager::GetInstance().CreateSignal<std::string>("Throttle Status", webSocketServer, get_signal_and_value_encoder<std::string>());
        SignalManager::GetInstance().CreateSignal<std::string>("Net RX", webSocketServer, get_signal_and_value_encoder<std::string>());
        SignalManager::GetInstance().CreateSignal<std::string>("Net TX", webSocketServer, get_signal_and_value_encoder<std::string>());
        SignalManager::GetInstance().CreateSignal<std::string>("Disk Usage", webSocketServer, get_signal_and_value_encoder<std::string>());
        SignalManager::GetInstance().CreateSignal<std::string>("Load Avg", webSocketServer, get_signal_and_value_encoder<std::string>());
        SignalManager::GetInstance().CreateSignal<std::string>("Uptime", webSocketServer, get_signal_and_value_encoder<std::string>());
    }
};