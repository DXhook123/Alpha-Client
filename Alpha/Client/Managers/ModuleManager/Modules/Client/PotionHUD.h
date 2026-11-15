#pragma once

#include "HudModule.h"

class PotionHUD : public HudModule {
public:
	PotionHUD();
	void onImGuiRender(ImDrawList* drawlist) override;

	float size = 1.f;
	float rowSpacing = 2.f;
	bool background = true;
};
