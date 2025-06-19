#pragma once

#include "../signals/signal.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <stdexcept>


class ColorMapper
{
    public:
        static RGB HSVtoRGB(float h, float s, float v)
        {
            float c = v * s;
            float x = c * (1 - std::fabs(std::fmod(h / 60.0f, 2) - 1));
            float m = v - c;

            float r1 = 0, g1 = 0, b1 = 0;
            if (h < 60)      { r1 = c; g1 = x; }
            else if (h < 120){ r1 = x; g1 = c; }
            else if (h < 180){ g1 = c; b1 = x; }
            else if (h < 240){ g1 = x; b1 = c; }
            else if (h < 300){ r1 = x; b1 = c; }
            else             { r1 = c; b1 = x; }

            return {
                static_cast<uint8_t>((r1 + m) * 255),
                static_cast<uint8_t>((g1 + m) * 255),
                static_cast<uint8_t>((b1 + m) * 255)
            };
        }

        static RGB normalizedToRGB(float n, float total, float amplitudeNormalized, ColorMappingType mapping = ColorMappingType::Linear)
        {
            float hue = 0.0f;

            switch (mapping)
            {
                case ColorMappingType::Linear:
                    hue = (n / total) * 360.0f;
                    break;
                case ColorMappingType::Log2:
                    hue = (std::log2(n + 1) / std::log2(total)) * 360.0f;
                    break;
                case ColorMappingType::Log10:
                    hue = (std::log10(n + 1) / std::log10(total)) * 360.0f;
                    break;
                default:
                    throw std::invalid_argument("Invalid color mapping type");
            }

            return HSVtoRGB(hue, 1.0f, amplitudeNormalized);
        }
};
