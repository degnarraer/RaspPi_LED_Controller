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

#include "../signal.h"
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
};
