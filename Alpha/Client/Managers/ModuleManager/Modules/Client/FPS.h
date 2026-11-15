#pragma once
#include "HudModule.h"

class FPS : public HudModule {
public:
	FPS() : HudModule("FPS", "Show frames per second", Category::Hud) {
		defaultPos = Vec2<float>(10.f, 34.f);
		position = defaultPos;
	}

	void onImGuiRender(ImDrawList* drawlist) override;
};
