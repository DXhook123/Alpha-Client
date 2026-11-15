#include "Position.h"
#include "sdk/Game.h"
#include "sdk/Classes/LocalPlayer.h"
#include <cstdio>
#include <cmath>

void Position::onImGuiRender(ImDrawList* drawlist) {
	LocalPlayer* lp = Game::getLocalPlayer();
	if (!lp) return;

	static Vec3<float> lastPos(0.f, 0.f, 0.f);
	static int lastDimension = -99;
	static std::string cachedCoords;

	Vec3<float> lpPos = lp->getPos();
	int dimensionId = -1;
	if (DimensionTypeComponent* dimComp = lp->getDimensionTypeComponent())
		dimensionId = dimComp->type;

	if (std::abs(lpPos.x - lastPos.x) > 0.1f ||
		std::abs(lpPos.y - lastPos.y) > 0.1f ||
		std::abs(lpPos.z - lastPos.z) > 0.1f ||
		dimensionId != lastDimension) {

		char xyzBuf[64];
		snprintf(xyzBuf, sizeof(xyzBuf), "%.0f, %.0f, %.0f", lpPos.x, (lpPos.y - 1.6f), lpPos.z);

		char otherBuf[64];
		if (dimensionId == 0) {
			snprintf(otherBuf, sizeof(otherBuf), "%.0f, %.0f, %.0f", (lpPos.x / 8.f), (lpPos.y - 1.6f), (lpPos.z / 8.f));
		}
		else if (dimensionId == 1) {
			snprintf(otherBuf, sizeof(otherBuf), "%.0f, %.0f, %.0f", (lpPos.x * 8.f), (lpPos.y - 1.6f), (lpPos.z * 8.f));
		}
		else {
			snprintf(otherBuf, sizeof(otherBuf), "0, 0, 0");
		}

		cachedCoords = std::string(xyzBuf) + " [" + std::string(otherBuf) + "]";
		lastPos = lpPos;
		lastDimension = dimensionId;
	}

	drawLabeledRect("XYZ", cachedCoords);
}
