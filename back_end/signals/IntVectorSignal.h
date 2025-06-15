#pragma once

#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "signal.h"
#include "../websocket_server.h"

class IntVectorSignal: public Signal<std::vector<int32_t>>
{
public:
    IntVectorSignal( const std::string& signalName
                   , std::shared_ptr<WebSocketServer> webSocketServer)                        
                   : Signal<std::vector<int32_t>>(signalName, webSocketServer, get_timestamped_int32_vector_to_binary_encoder())
    {
        SignalManager::getInstance().createSignal(signalName, webSocketServer, get_timestamped_int32_vector_to_binary_encoder());
    }

private:

    BinaryEncoder<std::vector<int32_t>> get_timestamped_int32_vector_to_binary_encoder() const
    {
        return [this](const std::string& name, const std::vector<int32_t>& vec) -> std::vector<uint8_t>
        {
            std::vector<uint8_t> buffer;

            // Message type
            buffer.push_back(static_cast<uint8_t>(BinaryEncoderType::Timestamped_Int_Vector_Encoder));

            // Name length (2 bytes, big-endian)
            uint16_t name_len = static_cast<uint16_t>(name.size());
            buffer.push_back((name_len >> 8) & 0xFF);
            buffer.push_back(name_len & 0xFF);

            // Signal name
            buffer.insert(buffer.end(), name.begin(), name.end());

            // Timestamp (8 bytes, big-endian)
            uint64_t timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

            for (int i = 7; i >= 0; --i)
            {
                buffer.push_back((timestamp >> (i * 8)) & 0xFF);
            }

            // Vector count (2 bytes, big-endian)
            uint16_t count = static_cast<uint16_t>(vec.size());
            buffer.push_back((count >> 8) & 0xFF);
            buffer.push_back(count & 0xFF);

            // Vector payload (4 bytes per int, big-endian)
            for (int32_t val : vec)
            {
                buffer.push_back((val >> 24) & 0xFF);
                buffer.push_back((val >> 16) & 0xFF);
                buffer.push_back((val >> 8) & 0xFF);
                buffer.push_back(val & 0xFF);
            }

            return buffer;
        };
    }


    JsonEncoder<std::vector<int32_t>> get_vector_to_json_encoder()
    {
        return [this](const std::string& name, const std::vector<int32_t>& vec) -> std::string
        {
            nlohmann::json j;
            j["type"] = "signal";
            j["signal"] = name;
            j["value"] = vec;
            return j.dump();
        };
    }
};
