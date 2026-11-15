#pragma once

#include "HudModule.h"

class WelcomeHUD : public HudModule {
public:
	WelcomeHUD();

	void onImGuiRender(ImDrawList* drawlist) override;

	std::string format = "Welcome back, %player%!";
	bool background = false;
	bool shadow = true;
	bool glow = true;
	float shadowStrength = .75f;
	float glowStrength = .9f;
	float glowSize = 3.5f;

private:
	std::string getDisplayText() const;
};
