#include "FPS.h"
#include <chrono>
#include <cstdio>

void FPS::onImGuiRender(ImDrawList* drawlist) {
	using namespace std::chrono;
	static int frameCount = 0;
	static int currentFPS = 0;
	static auto lastTime = steady_clock::now();

	frameCount++;
	auto now = steady_clock::now();
	if (duration<float>(now - lastTime).count() >= 1.0f) {
		currentFPS = frameCount;
		frameCount = 0;
		lastTime = now;
	}

	char fpsBuf[16];
	snprintf(fpsBuf, sizeof(fpsBuf), "%i", currentFPS);
	drawLabeledRect("FPS", std::string(fpsBuf));
}
