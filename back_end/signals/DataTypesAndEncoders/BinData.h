#pragma once

#include <nlohmann/json.hpp>
using json = nlohmann::json;

struct BinData
{
    uint16_t minBin = 0;
    uint16_t maxBin = 0;
    uint16_t totalBins = 0;
    float normalizedMinValue = 0.0;
    float normalizedMaxValue = 0.0;

    bool operator==(const BinData& other) const
    {
        return minBin == other.minBin &&
            maxBin == other.maxBin &&
            totalBins == other.totalBins &&
            normalizedMinValue == other.normalizedMinValue &&
            normalizedMaxValue == other.normalizedMaxValue;
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
        {"normalizeMinValue", data.normalizedMinValue},
        {"normalizeMaxValue", data.normalizedMaxValue}
    };
}

inline void from_json(const json& j, BinData& data)
{
    j.at("minBin").get_to(data.minBin);
    j.at("maxBin").get_to(data.maxBin);
    j.at("totalBins").get_to(data.totalBins);
    j.at("normalizeMinValue").get_to(data.normalizedMinValue);
    j.at("normalizeMaxValue").get_to(data.normalizedMaxValue);
}

inline std::ostream& operator<<(std::ostream& os, const BinData& data)
{
    os << "BinData{minBin=" << data.minBin
       << ", maxBin=" << data.maxBin
       << ", totalBins=" << data.totalBins
       << ", normalizedMinValue=" << data.normalizedMinValue
       << ", normalizedMaxValue=" << data.normalizedMaxValue
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
    if (!parseKeyValue("normalizeMinValue", data.normalizedMinValue)) { is.setstate(std::ios::failbit); return is; }
    if (!parseKeyValue("normalizeMaxValue", data.normalizedMaxValue, true)) { is.setstate(std::ios::failbit); return is; }

    return is;
}
