#pragma once

#include "../Module.h"
#include "Utils/ColorUtil.h"

#include <mutex>
#include <string>
#include <vector>

class SkinImage;
class TabList : public Module {
public:
	TabList();

	bool runOnBackground() override { return true; }
	void onEnable() override;
	void onDisable() override;
	void onNormalTick(LocalPlayer* localPlayer) override;
	void onImGuiRender(ImDrawList* drawlist) override;

private:
	struct PlayerSnapshot {
		std::string name;
		std::vector<unsigned char> headRgba;
	};

	static std::string stripFormatting(const std::string& value);
	static std::vector<unsigned char> makePlayerHeadRgba(const SkinImage& image);

	float textSize = 0.9f;
	float rowSpacing = 5.f;
	bool background = true;
	UIColor textColor{ 255, 255, 255, 255 };
	UIColor backgroundColor{ 0, 0, 0, 150 };
	UIColor rowBoxColor{ 90, 150, 255, 220 };

	mutable std::mutex playersMutex;
	std::vector<PlayerSnapshot> players;
};
