#pragma once

#include "../Module.h"
#include "sdk/Classes/BlockPos.h"
#include "Utils/Maths.h"
#include <chrono>
#include <optional>
#include <vector>

class Actor;

class FeetTrap : public Module {
public:
	FeetTrap();

	void onEnable() override;
	void onDisable() override;
	void onNormalTick(LocalPlayer* localPlayer) override;
	void onUpdateRotation(LocalPlayer* localPlayer) override;
	void onSendPacket(Packet* packet, bool& cancel) override;
	void onLevelRender() override;

private:
	int distance = 1;
	int delay = 50;
	int blocks = 4;
	bool swing = false;
	bool jumpDisable = true;
	bool pauseOnUse = false;
	bool attackCrystal = true;
	bool render = false;

	std::vector<BlockPos> toPlace;
	std::vector<Actor*> toBreak;
	std::optional<Vec2<float>> rotate;
	Vec2<float> curRotate{ 0.f, 0.f };
	bool sneaking = false;
	std::chrono::steady_clock::time_point lastPlaceTime;

	void findBlocks();
};
