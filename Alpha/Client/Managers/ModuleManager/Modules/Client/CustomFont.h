#pragma once
#include "../Module.h"

class CustomFont : public Module {
public:
	int fontType = 0;
	int fontStyle = 0;
	int sizePreset = 2;
	bool fontShadow = true;
	float shadowSize = 1.f;
	float fontSize = 1.f;

	CustomFont();

	bool isEnabled() const override { return true; }
	void setEnabled(bool enabled) override {}
};
