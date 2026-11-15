#pragma once
#include "../../Utils/Maths.h"

struct StateVectorComponent {
	Vec3<float> mPos;
	Vec3<float> mPosPrev;
	Vec3<float> mPosDelta;
};
