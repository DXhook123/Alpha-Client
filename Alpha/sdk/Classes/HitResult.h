#pragma once
#include "../../Utils/MemoryUtil.h"
#include "../../Utils/Maths.h"
#include "BlockPos.h"

enum class HitResultType : int {
	Tile = 0,
	Entity = 1,
	EntityOutOfRange = 2,
	NoHit = 3
};

class HitResult {
public:
	Vec3<float> mStartPos;
	Vec3<float> mRayDir;
	HitResultType mType;
	uint8_t mFacing;
	BlockPos mBlock;
	Vec3<float> mPos;
};
