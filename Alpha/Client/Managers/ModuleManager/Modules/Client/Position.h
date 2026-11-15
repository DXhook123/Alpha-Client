#pragma once
#include "HudModule.h"

class Position : public HudModule {
public:
	Position() : HudModule("Position", "Show your coordinates", Category::Hud) {
		defaultPos = Vec2<float>(6.f, 100000.f);
		position = defaultPos;
	}

	void onImGuiRender(ImDrawList* drawlist) override;
};
