#pragma once
#include "HudModule.h"
#include <atomic>
#include <chrono>
#include <cstdio>

class TPSHUD : public HudModule {
public:
	TPSHUD() : HudModule("TPS", "Show the measured local game tick rate", Category::Hud) {}

	void onNormalTick(LocalPlayer* player) override {
		if (!player) {
			started = false;
			displayTps.store(0.f, std::memory_order_relaxed);
			return;
		}
		const auto now = std::chrono::steady_clock::now();
		if (!started) {
			started = true;
			sampleStart = now;
			ticks = 0;
			return;
		}
		++ticks;
		const float seconds = std::chrono::duration<float>(now - sampleStart).count();
		if (seconds >= 1.f) {
			displayTps.store(static_cast<float>(ticks) / seconds, std::memory_order_relaxed);
			ticks = 0;
			sampleStart = now;
		}
	}

	void onImGuiRender(ImDrawList*) override {
		char value[24];
		std::snprintf(value, sizeof(value), "%.2f", displayTps.load(std::memory_order_relaxed));
		drawLabeledRect("TPS", value);
	}

private:
	std::atomic<float> displayTps{ 0.f };
	std::chrono::steady_clock::time_point sampleStart{};
	int ticks = 0;
	bool started = false;
};
