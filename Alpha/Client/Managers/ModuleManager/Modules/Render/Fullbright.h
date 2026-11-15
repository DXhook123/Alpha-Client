#pragma once
#include "../Module.h"
class Fullbright : public Module {
public:
	int brightness = 10;

	Fullbright();
	void onEnable() override;
	void onDisable() override;
};
