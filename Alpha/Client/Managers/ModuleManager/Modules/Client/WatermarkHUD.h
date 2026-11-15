#pragma once

#include "HudModule.h"

class WatermarkHUD : public HudModule {
public:
	WatermarkHUD();

	void onImGuiRender(ImDrawList* drawlist) override;

	std::string text = "%client%";
	bool background = false;
	bool pulse = true;
	bool shadow = true;
	bool glow = true;
	float shadowStrength = 0.75f;
	float glowStrength = 0.9f;
	float glowSize = 3.5f;

private:
	std::string getDisplayText() const;
};
