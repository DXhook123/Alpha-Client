#pragma once

#include "../Module.h"
class DebugInfo : public Module {
public:
	DebugInfo();

	void onImGuiRender(ImDrawList* drawlist) override;

private:
	float textSize = 0.9f;
};
