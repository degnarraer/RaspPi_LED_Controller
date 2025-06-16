
#pragma once

#include "BinData.h"
#include "Point.h"
#include "Encoder_Binary.h"
#include "Encoder_Json.h"
#include "Encoder_String.h"
#include "../../websocket_session.h"

//************** ColorMappingType //***************/
enum class ColorMappingType
{
    Linear,
    Log2,
    Log10,
};

inline std::string to_string(ColorMappingType type)
{
    switch (type)
    {
        case ColorMappingType::Linear: return "Linear";
        case ColorMappingType::Log2:   return "Log2";
        case ColorMappingType::Log10:  return "Log10";
        default: throw std::invalid_argument("Unknown ColorMappingType");
    }
}

inline std::ostream& operator<<(std::ostream& os, ColorMappingType type)
{
    switch (type)
    {
        case ColorMappingType::Linear: os << "Linear"; break;
        case ColorMappingType::Log2:   os << "Log2";   break;
        case ColorMappingType::Log10:  os << "Log10";  break;
        default:                       os.setstate(std::ios::failbit); break;
    }
    return os;
}

inline std::istream& operator>>(std::istream& is, ColorMappingType& type)
{
    std::string token;
    is >> token;

    if (token == "Linear")       type = ColorMappingType::Linear;
    else if (token == "Log2")    type = ColorMappingType::Log2;
    else if (token == "Log10")   type = ColorMappingType::Log10;
    else                         is.setstate(std::ios::failbit);

    return is;
}

template<typename T>
JsonEncoder<T> get_signal_and_value_encoder()
{
    static_assert(std::is_constructible<json, T>::value,
        "T must be serializable to nlohmann::json");
    const JsonEncoder<T> encoder = [](const std::string& signal, const T& value) {
        json j;
        j["type"] = MessageTypeHelper::type_to_string_.at(MessageTypeHelper::MessageType::Signal_Value_Message);
        j["signal"] = signal;
        j["value"] = value;
        return j.dump();
    };
    return encoder;
}

template <typename T>
inline json encode_labels_with_values(const std::vector<std::string>& labels, const std::vector<T>& values)
{
    static_assert(std::is_constructible<json, std::vector<T>>::value,
        "T must be serializable to nlohmann::json");

    if (labels.size() != values.size())
    {
        throw std::invalid_argument("Labels and values vectors must have the same size.");
    }

    json j;
    j["labels"] = labels;
    j["values"] = values;
    return j;
}

inline std::string encode_signal_name_and_json(const std::string& signal, const json& value)
{
    json j;
    j["type"] = MessageTypeHelper::type_to_string_.at(MessageTypeHelper::MessageType::Signal_Value_Message);
    j["signal"] = signal;
    j["value"] = value;
    return j.dump();
}

inline JsonEncoder<std::vector<float>> get_fft_bands_encoder()
{
    return [](const std::string& signal, const std::vector<float>& values) -> std::string {
        std::vector<std::string> labels = {
            "16 Hz", "20 Hz", "25 Hz", "31.5 Hz", "40 Hz", "50 Hz", "63 Hz", "80 Hz", "100 Hz",
            "125 Hz", "160 Hz", "200 Hz", "250 Hz", "315 Hz", "400 Hz", "500 Hz", "630 Hz", "800 Hz", "1000 Hz", "1250 Hz",
            "1600 Hz", "2000 Hz", "2500 Hz", "3150 Hz", "4000 Hz", "5000 Hz", "6300 Hz", "8000 Hz", "10000 Hz", "12500 Hz",
            "16000 Hz", "20000 Hz"
        };
        json j = encode_labels_with_values(labels, values);
        return encode_signal_name_and_json(signal, j);
    };
}

inline JsonEncoder<BinData> get_bin_data_encoder()
{
    return [](const std::string& signal, const BinData& data) -> std::string {
        json j = data;
        return encode_signal_name_and_json(signal, j);
    };
}

struct Color
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

inline std::ostream& operator<<(std::ostream& os, const Color& c)
{
    os << '(' << static_cast<int>(c.r) << ',' << static_cast<int>(c.g) << ',' << static_cast<int>(c.b) << ')';
    return os;
}

inline std::istream& operator>>(std::istream& is, Color& c)
{
    char ch1, ch2, ch3, ch4;
    int r, g, b; // read as int first to avoid char interpretation

    if (is >> ch1 && ch1 == '(' &&
        is >> r >> ch2 && ch2 == ',' &&
        is >> g >> ch3 && ch3 == ',' &&
        is >> b >> ch4 && ch4 == ')')
    {
        if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255)
        {
            is.setstate(std::ios::failbit);
            return is;
        }

        c.r = static_cast<uint8_t>(r);
        c.g = static_cast<uint8_t>(g);
        c.b = static_cast<uint8_t>(b);

        return is;
    }

    is.setstate(std::ios::failbit);
    return is;
}

struct Pixel
{
    Color color;
    float brightness = 1.0f;        // 0.0 to 1.0
    uint8_t device_brightness = 31; // 0-31
};

inline std::ostream& operator<<(std::ostream& os, const Pixel& p)
{
    os << "{color=" << p.color
       << ", brightness=" << p.brightness
       << ", device_brightness=" << static_cast<int>(p.device_brightness)
       << '}';
    return os;
}

inline std::istream& operator>>(std::istream& is, Pixel& p)
{
    std::string token;

    if (!(is >> token) || token != "{color=")
    {
        is.setstate(std::ios::failbit);
        return is;
    }

    if (!(is >> p.color))
    {
        is.setstate(std::ios::failbit);
        return is;
    }

    if (!(is >> token) || token != ",")
    {
        is.setstate(std::ios::failbit);
        return is;
    }

    if (!(is >> token) || token.substr(0, 11) != "brightness=")
    {
        is.setstate(std::ios::failbit);
        return is;
    }

    // parse brightness value after =
    std::string brightnessStr = token.substr(11);
    std::istringstream brightnessStream(brightnessStr);
    if (!(brightnessStream >> p.brightness))
    {
        is.setstate(std::ios::failbit);
        return is;
    }

    if (!(is >> token) || token != ",")
    {
        is.setstate(std::ios::failbit);
        return is;
    }

    if (!(is >> token) || token.substr(0, 17) != "device_brightness=")
    {
        is.setstate(std::ios::failbit);
        return is;
    }

    std::string deviceBrightnessStr = token.substr(17);
    int devBrightInt;
    std::istringstream deviceBrightnessStream(deviceBrightnessStr);
    if (!(deviceBrightnessStream >> devBrightInt))
    {
        is.setstate(std::ios::failbit);
        return is;
    }
    p.device_brightness = static_cast<uint8_t>(devBrightInt);

    if (!(is >> token) || token != "}")
    {
        is.setstate(std::ios::failbit);
        return is;
    }

    return is;
}
