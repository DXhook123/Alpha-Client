#pragma once
#include "HudModule.h"

class PingHUD : public HudModule {
public:
	PingHUD() : HudModule("Ping", "Show your average network latency", Category::Hud) {
		defaultPos = Vec2<float>(10.f, 82.f);
		position = defaultPos;
	}

	void onImGuiRender(ImDrawList* drawlist) override;
};
