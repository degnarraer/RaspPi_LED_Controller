#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <ostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <spdlog/spdlog.h>

#include "signal.h"
#include "../websocket_server.h"

struct RGB
{
    uint8_t r;
    uint8_t g;
    uint8_t b;

    bool operator==(const RGB& other) const { return r == other.r && g == other.g && b == other.b; }
    bool operator!=(const RGB& other) const { return !(*this == other); }
};

inline std::string to_hex_string(const RGB& color)
{
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(color.r)
        << std::setw(2) << static_cast<int>(color.g)
        << std::setw(2) << static_cast<int>(color.b);
    return oss.str();
}

inline void to_json(nlohmann::json& j, const RGB& color)
{
    j = to_hex_string(color);
}

inline void from_json(const nlohmann::json& j, RGB& color)
{
    std::string hex_str = j.get<std::string>();
    if (hex_str.size() == 6)
    {
        color.r = std::stoi(hex_str.substr(0, 2), nullptr, 16);
        color.g = std::stoi(hex_str.substr(2, 2), nullptr, 16);
        color.b = std::stoi(hex_str.substr(4, 2), nullptr, 16);
    }
    else
    {
        throw std::invalid_argument("Invalid hex string for RGB color");
    }
}

inline std::ostream& operator<<(std::ostream& os, const RGB& color)
{
    return os << "RGB(" << static_cast<int>(color.r) << ", "
                      << static_cast<int>(color.g) << ", "
                      << static_cast<int>(color.b) << ")";
}

inline std::string to_string(const RGB& color)
{
    return to_hex_string(color);
}

class PixelGridSignal
{
public:
    PixelGridSignal(const std::string& signalName,
                    size_t width,
                    size_t height,
                    std::shared_ptr<WebSocketServer> webSocketServer);

    void SetPixel(size_t x, size_t y, RGB color);
    RGB GetPixel(size_t x, size_t y) const;

    void Clear(RGB color = {0, 0, 0});
    void Notify();

    std::shared_ptr<Signal<std::vector<std::vector<RGB>>>> GetSignal() const;

    size_t GetWidth() const { return width_; }
    size_t GetHeight() const { return height_; }

private:
    std::string signalName_;
    size_t width_;
    size_t height_;
    std::shared_ptr<WebSocketServer> webSocketServer_;
    std::vector<std::vector<RGB>> pixels_;
    std::shared_ptr<Signal<std::vector<std::vector<RGB>>>> signal_;
    std::shared_ptr<spdlog::logger> logger_;
    
    BinaryEncoder<std::vector<std::vector<RGB>>> get_rgb_matrix_to_binary_encoder()
    {
        const BinaryEncoder<std::vector<std::vector<RGB>>> encoder = [this](const std::string& signal_name, const std::vector<std::vector<RGB>>& matrix) -> std::vector<uint8_t>
        {
            std::vector<uint8_t> buffer;

            // Message type
            buffer.push_back(static_cast<uint8_t>(BinaryEncoderType::Named_Binary_Encoder));

            // Signal name
            uint16_t name_len = static_cast<uint16_t>(signal_name.size());
            buffer.push_back((name_len >> 8) & 0xFF);
            buffer.push_back(name_len & 0xFF);
            buffer.insert(buffer.end(), signal_name.begin(), signal_name.end());
            
            // Matrix dimensions
            uint16_t rows = static_cast<uint16_t>(matrix.size());
            uint16_t cols = rows > 0 ? static_cast<uint16_t>(matrix[0].size()) : 0;
            buffer.push_back((rows >> 8) & 0xFF);
            buffer.push_back(rows & 0xFF);
            buffer.push_back((cols >> 8) & 0xFF);
            buffer.push_back(cols & 0xFF);

            // Flatten and write RGB values row-major
            for (const auto& row : matrix)
            {
                for (const RGB& pixel : row)
                {
                    buffer.push_back(pixel.r);
                    buffer.push_back(pixel.g);
                    buffer.push_back(pixel.b);
                }
            }
            return buffer;
        };
        return encoder;
    }

    JsonEncoder<std::vector<std::vector<RGB>>> get_rgb_matrix_to_json_encoder()
    {
        const JsonEncoder<std::vector<std::vector<RGB>>> encoder = [this](const std::string& signal, const std::vector<std::vector<RGB>>& value) {
            json j;
            j["type"] = "signal";
            j["signal"] = signal;

            json value_json = json::array();
            for (const auto& row : value) {
                json row_json = json::array();
                for (const auto& rgb : row) {
                    row_json.push_back(to_hex_string(rgb));
                }
                value_json.push_back(row_json);
            }
            j["value"] = value_json;
            return j.dump();
        };
        return encoder;
    }
};
