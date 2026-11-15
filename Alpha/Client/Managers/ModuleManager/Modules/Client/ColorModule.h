#pragma once
#include "../Module.h"
#include "../Settings/ColorSetting.h"
#include "../Settings/EnumSetting.h"
#include "../Settings/SliderSetting.h"
class ColorModule : public Module {
public:
	UIColor themeColor;

	UIColor waveColor1 = UIColor(132, 91, 245, 255);
	UIColor waveColor2 = UIColor(0, 180, 255, 255);
	UIColor topColor = UIColor(0, 120, 212, 255);
	int presetMode = 0;
	int colorMode = 0;
	float rainbowSpeed = 4.f;
	float saturation = 130.f;
	int rainbowDelay = 350;

	ColorModule() : Module("Color", "Global theme color setting", Category::CLIENT, 0) {
		s_instance = this;
		themeColor = UIColor(255, 255, 255, 255);
		registerSetting(new EnumSetting("Preset", "One-click theme presets",
			{ "Custom", "Trans Flag", "Sakura", "Lavender", "Neon Night" }, &presetMode, 0));
		registerSetting(new ColorSetting("Theme", "Main theme color", &themeColor,
			UIColor(255, 255, 255, 255), [this] { return presetMode == 0 && colorMode == 0; }));

		registerSetting(new EnumSetting("ColorMode", "Shared GUI and HUD color mode",
			{ "Custom", "Rainbow", "Astolfo", "Wave", "RGBWave" }, &colorMode, 0));
		registerSetting(new ColorSetting("Color", "Category header color in Custom mode",
			&topColor, UIColor(0, 120, 212, 255), [this]() { return presetMode == 0 && colorMode == 0; }));
		registerSetting(new SliderSetting<float>("RainbowSpeed", "Shared dynamic color speed",
			&rainbowSpeed, 4.f, 1.f, 10.f, [this]() { return colorMode != 0; }));
		registerSetting(new SliderSetting<float>("Saturation", "Rainbow and Astolfo saturation",
			&saturation, 130.f, 0.f, 255.f, [this]() { return colorMode == 1 || colorMode == 2 || colorMode == 4; }));
		registerSetting(new SliderSetting<int>("Delay", "Dynamic color offset by GUI or HUD position",
			&rainbowDelay, 350, 0, 1000, [this]() { return colorMode != 0; }));
		registerSetting(new ColorSetting("Wave Color 1", "First color for Wave and RGBWave", &waveColor1, waveColor1,
			[this] { return colorMode == 3 || colorMode == 4; }));
		registerSetting(new ColorSetting("Wave Color 2", "Second color for Wave and RGBWave", &waveColor2, waveColor2,
			[this] { return colorMode == 3 || colorMode == 4; }));
	}
    ~ColorModule() override { if (s_instance == this) s_instance = nullptr; }
    static ColorModule* instance() { return s_instance; }
    static bool hasDynamicColors() { return s_instance && s_instance->colorMode != 0; }
    static UIColor resolveColor(const UIColor& custom, double offset = 0.0);
	bool runOnBackground() override { return true; }
	void setEnabled(bool enable) override { Module::setEnabled(true); }
	void toggle() override {}
	UIColor getColor(double delay = 0.0) const;
	static UIColor getThemeColor();
private:
    inline static ColorModule* s_instance = nullptr;
};
