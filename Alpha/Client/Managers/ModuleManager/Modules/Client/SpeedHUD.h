#pragma once
#include "HudModule.h"
#include <atomic>

class SpeedHUD : public HudModule {
public:
	SpeedHUD() : HudModule("Speed", "Show your horizontal speed", Category::Hud) {
		defaultPos = Vec2<float>(10.f, 58.f);
		position = defaultPos;
	}

	void onNormalTick(LocalPlayer* localPlayer) override;
	void onImGuiRender(ImDrawList* drawlist) override;

private:
	Vec3<float> lastPos{};
	bool hasLastPos = false;
	float smoothedSpeed = 0.f;
	std::atomic<float> displaySpeed{ 0.f };
};
