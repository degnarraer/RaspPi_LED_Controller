#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <ostream>
#include "../signal.h"
#include "../websocket_server.h"

struct RGB
{
    uint8_t r;
    uint8_t g;
    uint8_t b;

    bool operator==(const RGB& other) const
    {
        return r == other.r && g == other.g && b == other.b;
    }

    bool operator!=(const RGB& other) const
    {
        return !(*this == other);
    }
};

inline void to_json(nlohmann::json& j, const RGB& color)
{
    j = nlohmann::json{
        {"r", color.r},
        {"g", color.g},
        {"b", color.b}
    };
}

inline void from_json(const nlohmann::json& j, RGB& color)
{
    j.at("r").get_to(color.r);
    j.at("g").get_to(color.g);
    j.at("b").get_to(color.b);
}

inline std::ostream& operator<<(std::ostream& os, const RGB& color)
{
    return os << "RGB(" << static_cast<int>(color.r) << ", "
                        << static_cast<int>(color.g) << ", "
                        << static_cast<int>(color.b) << ")";
}

inline std::string to_string(const RGB& color)
{
    std::ostringstream oss;
    oss << "RGB(" << static_cast<int>(color.r) << ", "
                  << static_cast<int>(color.g) << ", "
                  << static_cast<int>(color.b) << ")";
    return oss.str();
}

class PixelGridSignal
{
public:
    PixelGridSignal( const std::string& baseSignalName
                   , size_t width
                   , size_t height 
                   , std::shared_ptr<WebSocketServer> webSocketServer);

    void SetPixel(size_t x, size_t y, RGB color);
    RGB GetPixel(size_t x, size_t y) const;

    size_t GetWidth() const { return width_; }
    size_t GetHeight() const { return height_; }

    void Clear(RGB color = {0, 0, 0});
    void Notify();
    void Notify(size_t row);

    std::shared_ptr<Signal<std::vector<RGB>>> GetRowSignal(size_t row) const;

private:
    std::string baseName_;
    size_t width_;
    size_t height_;
    std::shared_ptr<WebSocketServer> webSocketServer_;
    std::vector<std::shared_ptr<Signal<std::vector<RGB>>>> rowSignals_;
};
