#pragma once

#include <nlohmann/json.hpp>
using json = nlohmann::json;

struct BinData
{
    uint16_t minBin = 0;
    uint16_t maxBin = 0;
    uint16_t totalBins = 0;
    float minValue = 0.0;
    float maxValue = 0.0;

    bool operator==(const BinData& other) const
    {
        return minBin == other.minBin &&
            maxBin == other.maxBin &&
            totalBins == other.totalBins &&
            minValue == other.minValue &&
            maxValue == other.maxValue;
    }

    bool operator!=(const BinData& other) const
    {
        return !(*this == other);
    }

};

inline void to_json(json& j, const BinData& data)
{
    j = json{
        {"minBin", data.minBin},
        {"maxBin", data.maxBin},
        {"totalBins", data.totalBins},
        {"minValue", data.minValue},
        {"maxValue", data.maxValue}
    };
}

inline void from_json(const json& j, BinData& data)
{
    j.at("minBin").get_to(data.minBin);
    j.at("maxBin").get_to(data.maxBin);
    j.at("totalBins").get_to(data.totalBins);
    j.at("minValue").get_to(data.minValue);
    j.at("maxValue").get_to(data.maxValue);
}

inline std::ostream& operator<<(std::ostream& os, const BinData& data)
{
    os << "BinData{minBin=" << data.minBin
       << ", maxBin=" << data.maxBin
       << ", totalBins=" << data.totalBins
       << ", minValue=" << data.minValue
       << ", maxValue=" << data.maxValue
       << "}";
    return os;
}

#include <istream>
#include <string>

inline std::istream& operator>>(std::istream& is, BinData& data)
{
    std::string token;

    // Expected format:
    // BinData{minBin=..., maxBin=..., totalBins=..., minValue=..., maxValue=...}

    // Read and validate "BinData{"
    if (!(is >> token) || token.substr(0, 8) != "BinData{")
    {
        is.setstate(std::ios::failbit);
        return is;
    }

    // Helper lambda to parse "key=value," pairs (last one without comma)
    auto parseKeyValue = [&](const std::string& key, auto& value, bool isLast = false) -> bool
    {
        if (!(is >> token)) return false; // key=value, or key=value}
        auto pos = token.find('=');
        if (pos == std::string::npos) return false;
        std::string keyStr = token.substr(0, pos);
        std::string valStr = token.substr(pos + 1);

        if (keyStr != key) return false;

        std::istringstream valStream(valStr);
        valStream >> value;
        if (valStream.fail()) return false;

        if (!isLast)
        {
            // Expect comma after this token (or attached at end)
            char ch = is.peek();
            if (ch == ',') is.get();
            else return false;
        }
        else
        {
            // Last field should end with '}'
            if (valStr.back() != '}')
            {
                char ch;
                is >> ch;
                if (ch != '}') return false;
            }
        }

        return true;
    };

    if (!parseKeyValue("minBin", data.minBin)) { is.setstate(std::ios::failbit); return is; }
    if (!parseKeyValue("maxBin", data.maxBin)) { is.setstate(std::ios::failbit); return is; }
    if (!parseKeyValue("totalBins", data.totalBins)) { is.setstate(std::ios::failbit); return is; }
    if (!parseKeyValue("minValue", data.minValue)) { is.setstate(std::ios::failbit); return is; }
    if (!parseKeyValue("maxValue", data.maxValue, true)) { is.setstate(std::ios::failbit); return is; }

    return is;
}
