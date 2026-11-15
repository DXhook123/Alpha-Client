#pragma once
#include "../../Utils/MemoryUtil.h"
#include "../../Utils/Maths.h"
#include "BlockType.h"

class Block {
public:
	CLASS_MEMBER(BlockType*, blockType, 0x68);
	CLASS_MEMBER(uint64_t, serializationIdHash, 0x108);
	CLASS_MEMBER(unsigned int, serializationIdHashForNetwork, 0x110);
	CLASS_MEMBER(unsigned int, networkId, 0x114);
	CLASS_MEMBER(unsigned short, data, 0x120);
	CLASS_MEMBER(bool, hasRuntimeId, 0x122);
	uint32_t* getRuntimeId() {
		return (uint32_t*)((uintptr_t)this + 0x114);
	}
};
