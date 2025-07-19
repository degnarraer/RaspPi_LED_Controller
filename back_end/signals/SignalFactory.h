#pragma once

#include "signal.h"
#include "IntVectorSignal.h"
#include "../websocket_server.h"
#include "DataTypesAndEncoders/DataTypesAndEncoders.h"

class SignalFactory
{
public:
    static void CreateSignals(std::shared_ptr<WebSocketServer> webSocketServer)
    {
        if (!webSocketServer)
        {
            throw std::invalid_argument("WebSocketServer cannot be null");
        }
        
        SignalManager& signalManager = SignalManager::getInstance();

        //Microphone Signals
        IntVectorSignal("Microphone Left Channel", webSocketServer);
        IntVectorSignal("Microphone Right Channel", webSocketServer);

        //System Signals
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

        //Rendering Signals        
        signalManager.createSignal<std::string>("Color Mapping Type", webSocketServer, get_signal_and_value_encoder<std::string>())->setValue(to_string(ColorMappingType::Linear));

        //Sensitivity and Threshold Signals
        signalManager.createSignal<float>("Min db", webSocketServer, get_signal_and_value_encoder<float>());
        signalManager.createSignal<float>("Max db", webSocketServer, get_signal_and_value_encoder<float>());

        //Brightness and Current Signals
        signalManager.createSignal<float>("Calculated Current", webSocketServer, get_signal_and_value_encoder<float>());
        signalManager.createSignal<uint32_t>("Current Limit", webSocketServer, get_signal_and_value_encoder<uint32_t>());
        signalManager.createSignal<float>("Brightness", webSocketServer, get_signal_and_value_encoder<float>());
        signalManager.createSignal<uint8_t>("LED Driver Limit", webSocketServer, get_signal_and_value_encoder<std::uint8_t>());

        //Render Frequency Signals
        signalManager.createSignal<float>("Minimum Render Frequency", webSocketServer, get_signal_and_value_encoder<float>());
        signalManager.createSignal<float>("Maximum Render Frequency", webSocketServer, get_signal_and_value_encoder<float>());
    }
};