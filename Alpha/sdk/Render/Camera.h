#pragma once
#include "../../Utils/Maths.h"

namespace Camera {
	void captureFrame();

	bool worldToScreen(const Vec3<float>& worldPos, Vec2<float>& outScreen);

	Vec3<float> getCameraPos();

	Vec2<float> getFov();

	bool getViewBasis(Vec3<float>& right, Vec3<float>& up, Vec3<float>& forward);
}
