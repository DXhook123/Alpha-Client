#pragma once
#include "../../Utils/Maths.h"

struct BlockPos : Vec3<int> {
	BlockPos(int32_t x = 0, int32_t y = 0, int32_t z = 0)
		: Vec3<int>(x, y, z) {}

	BlockPos(const Vec3<int>& v) : Vec3<int>(v) {}

	BlockPos operator+(const BlockPos& other) const {
		return { x + other.x, y + other.y, z + other.z };
	}

	BlockPos operator-(const BlockPos& other) const {
		return { x - other.x, y - other.y, z - other.z };
	}

	bool operator==(const BlockPos& other) const {
		return x == other.x && y == other.y && z == other.z;
	}

	bool operator!=(const BlockPos& other) const {
		return !(*this == other);
	}
};
static_assert(sizeof(BlockPos) == 12, "BlockPos size mismatch");
