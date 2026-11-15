#pragma once
#include <string>
#include <stdint.h>
#include <d2d1.h>
#include <d2d1_3.h>
#include <algorithm>
#include <vector>
#include <cmath>

#include <imgui.h>
struct UIColor {
    union {
        struct {
            uint8_t r, g, b, a;
        };
        int arr[4];
    };

    static UIColor FromHSV(float h, float s, float v, float a = 1.0f) {
        h = fmodf(h, 360.0f);
        if (h < 0) h += 360.0f;

        s = std::clamp(s, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);

        float c = v * s;
        float x = c * (1.0f - std::abs(fmodf(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;

        float r1, g1, b1;
        if (h < 60.0f) { r1 = c; g1 = x; b1 = 0; }
        else if (h < 120.0f) { r1 = x; g1 = c; b1 = 0; }
        else if (h < 180.0f) { r1 = 0; g1 = c; b1 = x; }
        else if (h < 240.0f) { r1 = 0; g1 = x; b1 = c; }
        else if (h < 300.0f) { r1 = x; g1 = 0; b1 = c; }
        else { r1 = c; g1 = 0; b1 = x; }

        return UIColor(
            static_cast<uint8_t>((r1 + m) * 255),
            static_cast<uint8_t>((g1 + m) * 255),
            static_cast<uint8_t>((b1 + m) * 255),
            static_cast<uint8_t>(a * 255)
        );
    }

    UIColor(uint8_t red = 255, uint8_t green = 255, uint8_t blue = 255, uint8_t alpha = 255) {
        this->r = red;
        this->g = green;
        this->b = blue;
        this->a = alpha;
    }

    bool operator==(const UIColor& other) const {
        return (r == other.r && g == other.g && b == other.b && a == other.a);
    }

    D2D1_COLOR_F toD2D1Color() const {
        return D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }

    ImColor toImColor() const {
        return ImColor(this->r, this->g, this->b, this->a);
    }
};

class ColorUtil {
public:
    inline static unsigned __int64 getCurrentMs() {
        FILETIME f;
        GetSystemTimeAsFileTime(&f);
        (unsigned long long)f.dwHighDateTime;
        unsigned __int64 nano =
            ((unsigned __int64)f.dwHighDateTime << 32LL) + (unsigned __int64)f.dwLowDateTime;
        return (nano - 116444736000000000LL) / 10000;
    }
    static uint32_t ColorToUInt(const UIColor& color) {
        return (static_cast<uint32_t>(color.a) << 24) | (static_cast<uint32_t>(color.r) << 16) |
            (static_cast<uint32_t>(color.g) << 8) | static_cast<uint32_t>(color.b);
    }
    static  UIColor HexStringToColor(const std::string& hexString) {
        std::string hex = hexString;
        if (hex.substr(0, 2) == "0x") {
            hex = hex.substr(2);
        }

        if (hex.length() != 8) {
            return UIColor(0, 0, 0, 255);
        }

        try {
            uint32_t colorValue = std::stoul(hex, nullptr, 16);
            return UIColor(static_cast<uint8_t>((colorValue >> 24) & 0xFF),
                static_cast<uint8_t>((colorValue >> 16) & 0xFF),
                static_cast<uint8_t>((colorValue >> 8) & 0xFF),
                static_cast<uint8_t>(colorValue & 0xFF));
        }
        catch (...) {
            return UIColor(0, 0, 0, 255);
        }
    }

    static std::string ColorToHexString(const UIColor& color);

    static void ColorConvertHSVtoRGB(float h, float s, float v, float& out_r, float& out_g,
        float& out_b) {
        if (s == 0.0f) {
            out_r = out_g = out_b = v;
            return;
        }

        h = fmodf(h, 1.0f) / (60.0f / 360.0f);
        int i = (int)h;
        float f = h - (float)i;
        float p = v * (1.0f - s);
        float q = v * (1.0f - s * f);
        float t = v * (1.0f - s * (1.0f - f));

        switch (i) {
        case 0:
            out_r = v;
            out_g = t;
            out_b = p;
            break;
        case 1:
            out_r = q;
            out_g = v;
            out_b = p;
            break;
        case 2:
            out_r = p;
            out_g = v;
            out_b = t;
            break;
        case 3:
            out_r = p;
            out_g = q;
            out_b = v;
            break;
        case 4:
            out_r = t;
            out_g = p;
            out_b = v;
            break;
        case 5:
        default:
            out_r = v;
            out_g = p;
            out_b = q;
            break;
        }
    }
    static UIColor lerp(const UIColor& start, const UIColor& end, float t);
    static UIColor lerpHSV(const UIColor& start, const UIColor& end, float t);

    static UIColor getRainbowColor(float seconds, float saturation, float brightness, long index) {
        float currentHue =
            (((getCurrentMs() + index) % (int)(seconds * 1000)) / (float)(seconds * 1000));
        float red, green, blue = 0.0f;
        ColorConvertHSVtoRGB(currentHue, saturation, brightness, red, green, blue);

        return UIColor((int)(red * 255.f), (int)(green * 255.f), (int)(blue * 255.f));
    }
    static UIColor getWaveColor(const UIColor& startColor, const UIColor& endColor, int index) {
        double offset = ((getCurrentMs() - index) / 8) / (double)120;
        double aids123 = ((getCurrentMs() - index) % 1000 / 1000.000);
        int aids1234 = ((getCurrentMs() - index) % 2000 / 2000.000) * 2;
        aids123 = aids1234 % 2 == 0 ? aids123 : 1 - aids123;
        double inverse_percent = 1 - aids123;
        int redPart = (int)(startColor.r * inverse_percent + endColor.r * aids123);
        int greenPart = (int)(startColor.g * inverse_percent + endColor.g * aids123);
        int bluePart = (int)(startColor.b * inverse_percent + endColor.b * aids123);
        return UIColor(redPart, greenPart, bluePart);
    }
    static UIColor getBreathingColor(const UIColor& color, float time, float speed = 1.0f);
    static UIColor getPulseColor(const UIColor& color, float time, float intensity = 1.0f);

    static UIColor getOceanWaveColor(float time, float phase = 0.0f);
    static UIColor getAquaGradient(float progress, float alpha = 1.0f);
    static UIColor getCrystalShine(float time, float phase = 0.0f);
    static UIColor getLiquidFlow(float time, float phase = 0.0f);
    static UIColor getPlasmaEffect(float time, float phase = 0.0f);
    static UIColor getSweepGradient(float progress, float baseHue = 200.0f);
    static UIColor getGlowPulse(const UIColor& baseColor, float time, float intensity = 1.0f);
    static UIColor getEnergyFlow(float time, float phase = 0.0f);
    static UIColor getNeonGlow(float time, float hue = 200.0f, float intensity = 1.0f);

    static UIColor getMultiGradient(const std::vector<UIColor>& colors, float progress);
    static UIColor getAquaThemeColor(int variant, float alpha = 1.0f);

    struct GradientStop {
        float position;
        UIColor color;
    };

    static ID2D1LinearGradientBrush* createLinearGradientBrush(
        ID2D1RenderTarget* renderTarget,
        const std::vector<GradientStop>& stops,
        const D2D1_POINT_2F& startPoint,
        const D2D1_POINT_2F& endPoint);

    static ID2D1RadialGradientBrush* createRadialGradientBrush(
        ID2D1RenderTarget* renderTarget,
        const std::vector<GradientStop>& stops,
        const D2D1_POINT_2F& center, float radiusX,
        float radiusY);

    static std::vector<GradientStop> getAquaGradientStops(float alpha = 1.0f);
    static std::vector<GradientStop> getOceanGradientStops(float time, float alpha = 1.0f);
    static std::vector<GradientStop> getSunsetGradientStops(float alpha = 1.0f);
    static std::vector<GradientStop> getNeonGradientStops(float time, float alpha = 1.0f);
};
