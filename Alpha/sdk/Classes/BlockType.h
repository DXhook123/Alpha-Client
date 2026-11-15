#pragma once
#include "../../Utils/MemoryUtil.h"
#include "../../Utils/Maths.h"
#include "../Core/HashedString.h"
#include "BlockPos.h"
#include <cstdint>
#include <string>

class BlockSource;

class BlockType {
public:
	CLASS_MEMBER(HashedString, mRawName, 0xE0);
	__declspec(property(get = getmSolid)) bool mSolid;
	inline bool getmSolid() const {
		const auto flags = *reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(this) + 0x164);
		return (flags & 0x02u) != 0;
	}
	CLASS_MEMBER(uint16_t, mID, 0x17E);

	bool mayPlaceOn(BlockSource& region, const BlockPos& pos) const {
		return MemoryUtil::callVirtualFunc<bool, BlockSource&, const BlockPos&>(
			68, const_cast<BlockType*>(this), region, pos);
	}
};
