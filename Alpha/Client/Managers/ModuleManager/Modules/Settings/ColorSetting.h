#pragma once
#include "Setting.h"
#include "Utils/ColorUtil.h"
#include <imgui.h>

class ColorSetting : public Setting {
public:
	UIColor* value;

	float hue = 0.f;
	float saturation = 1.f;
	float brightness = 1.f;
	float alpha = 1.f;

	float dispHue = 0.f;
	float dispSat = 1.f;
	float dispBright = 1.f;
	float dispAlpha = 1.f;

	bool isExpanded = false;
	float expandAnim = 0.f;

	ColorSetting(std::string settingName, std::string des, UIColor* ptr,
		UIColor defaultValue,
		std::optional<std::function<bool(void)>> _dependOn = std::nullopt) {
		this->name = settingName;
		this->description = des;
		this->value = ptr;
		*this->value = defaultValue;
		this->type = SettingType::COLOR_S;
		this->dependOn = _dependOn;
		syncHSVFromColor();
		dispHue = hue;
		dispSat = saturation;
		dispBright = brightness;
		dispAlpha = alpha;
	}
	void syncHSVFromColor() {
		float r = value->r / 255.f, g = value->g / 255.f, b = value->b / 255.f;
		ImGui::ColorConvertRGBtoHSV(r, g, b, hue, saturation, brightness);
		alpha = value->a / 255.f;
	}

	void syncColorFromHSV() {
		float r, g, b;
		ImGui::ColorConvertHSVtoRGB(hue, saturation, brightness, r, g, b);
		*value = UIColor(
			static_cast<uint8_t>(r * 255.f),
			static_cast<uint8_t>(g * 255.f),
			static_cast<uint8_t>(b * 255.f),
			static_cast<uint8_t>(std::clamp(alpha, 0.f, 1.f) * 255.f)
		);
	}
	void tickAnim(bool dragging) {
		float target = isExpanded ? 1.f : 0.f;
		expandAnim = Math::animate(target, expandAnim, 0.18f);
		if (fabsf(target - expandAnim) < 0.005f) expandAnim = target;

		if (dragging) {
			dispHue = hue;
			dispSat = saturation;
			dispBright = brightness;
			dispAlpha = alpha;
		} else {
			dispHue = Math::lerp(dispHue, hue, 0.30f);
			dispSat = Math::lerp(dispSat, saturation, 0.30f);
			dispBright = Math::lerp(dispBright, brightness, 0.30f);
			dispAlpha = Math::lerp(dispAlpha, alpha, 0.30f);
		}
	}
};
