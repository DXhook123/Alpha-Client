#pragma once
#include <cstdint>

struct InsideBlockComponent {
	const void* mInsideBlock;
	int32_t     mBlockX;
	int32_t     mBlockY;
	int32_t     mBlockZ;
};
