#include "SpeedHUD.h"
#include "sdk/Game.h"
#include "sdk/Classes/LocalPlayer.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

void SpeedHUD::onNormalTick(LocalPlayer* localPlayer) {
	if (!localPlayer) {
		hasLastPos = false;
		smoothedSpeed = 0.f;
		displaySpeed.store(0.f, std::memory_order_release);
		return;
	}

	const Vec3<float> pos = localPlayer->getPos();
	if (!hasLastPos) {
		lastPos = pos;
		hasLastPos = true;
		smoothedSpeed = 0.f;
		displaySpeed.store(0.f, std::memory_order_release);
		return;
	}

	const float dx = pos.x - lastPos.x;
	const float dz = pos.z - lastPos.z;
	const float dist = sqrtf(dx * dx + dz * dz);
	lastPos = pos;

	if (!std::isfinite(dist) || dist > 16.f) {
		smoothedSpeed = 0.f;
		displaySpeed.store(0.f, std::memory_order_release);
		return;
	}

	const float measuredSpeed = dist * 20.f;
	if (smoothedSpeed == 0.f)
		smoothedSpeed = measuredSpeed;
	else
		smoothedSpeed += (measuredSpeed - smoothedSpeed) * 0.1f;

	if (!std::isfinite(smoothedSpeed))
		smoothedSpeed = 0.f;
	displaySpeed.store(smoothedSpeed, std::memory_order_release);
}

void SpeedHUD::onImGuiRender(ImDrawList* drawlist) {
	(void)drawlist;
	if (!Game::getLocalPlayer()) return;

	const float displayMs = displaySpeed.load(std::memory_order_acquire);
	char buf[32];
	snprintf(buf, sizeof(buf), "%.1f m/s", std::isfinite(displayMs) ? displayMs : 0.f);
	drawLabeledRect("Speed", std::string(buf));
}
