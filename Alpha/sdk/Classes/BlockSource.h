#pragma once
#include "../../Utils/MemoryUtil.h"
#include "../Signatures.h"
#include "Block.h"
#include "BlockPos.h"
#include "HitResult.h"

class Dimension;
class Level;
class ChunkSource;
class Actor;

class BlockSource {
public:
	CLASS_MEMBER(std::thread::id, mOwnerThreadID, 0x18);
	CLASS_MEMBER(bool, mAllowUnpopulatedChunks, 0x1C);
	CLASS_MEMBER(bool, mPublicSource, 0x1D);
	CLASS_MEMBER(Level*, mLevel, 0x20);
	CLASS_MEMBER(ChunkSource*, mChunkSource, 0x28);
	CLASS_MEMBER(Dimension*, mDimension, 0x30);
	CLASS_MEMBER(int16_t, mMaxHeight, 0x38);
	CLASS_MEMBER(int16_t, mMinHeight, 0x3A);
	CLASS_MEMBER(bool, mAllowTickingChanges, 0x58);
	CLASS_MEMBER(BlockPos, mPlaceChunkPos, 0x5C);
	CLASS_MEMBER(bool, mIsPersistantBlockSource, 0x80);
	virtual ~BlockSource() = default;

	Block* getBlock(const BlockPos& pos) {
		return MemoryUtil::callVirtualFunc<Block*>(2, this, pos);
	}

	Block* getBlock(int x, int y, int z) {
		return MemoryUtil::callVirtualFunc<Block*>(1, this, x, y, z);
	}

	void* getBlockEntity(const BlockPos& pos) {
		return MemoryUtil::callVirtualFunc<void*>(4, this, pos);
	}

	bool hasBlock(const BlockPos& pos) {
		return MemoryUtil::callVirtualFunc<bool>(7, this, pos);
	}

	void* getDimensionId() {
		return MemoryUtil::callVirtualFunc<void*>(15, this);
	}

	void fetchAABBs(std::vector<AABB>& out, const AABB& region, bool expand) {
		MemoryUtil::callVirtualFunc<void>(16, this, out, region, expand);
	}

	void fetchCollisionShapes(std::vector<AABB>& out, const AABB& region, float* maxVerticalDist, bool ignoreUnloaded, void* optionalRef) {
		MemoryUtil::callVirtualFunc<void>(17, this, out, region, maxVerticalDist, ignoreUnloaded, optionalRef);
	}

	void* getTallestCollisionShape(const AABB& region, float* maxVerticalDist, bool ignoreUnloaded, void* optionalRef) {
		return MemoryUtil::callVirtualFunc<void*>(18, this, region, maxVerticalDist, ignoreUnloaded, optionalRef);
	}

	void setBlock(const BlockPos& pos, const Block& block, int updateFlags, void* syncMsg, Actor* source) {
		MemoryUtil::callVirtualFunc<void>(25, this, pos, block, updateFlags, syncMsg, source);
	}

	int getMaxHeight() {
		return MemoryUtil::callVirtualFunc<int>(26, this);
	}

	int getMinHeight() {
		return MemoryUtil::callVirtualFunc<int>(27, this);
	}
	Dimension* getDimension() {
		return MemoryUtil::callVirtualFunc<Dimension*>(28, this);
	}

	void* getChunkAt(const BlockPos& pos) {
		return MemoryUtil::callVirtualFunc<void*>(31, this, pos);
	}

	void* getILevel() {
		return MemoryUtil::callVirtualFunc<void*>(32, this);
	}
	bool areChunksFullyLoaded(const BlockPos& pos, int radius) {
		return MemoryUtil::callVirtualFunc<bool>(38, this, pos, radius);
	}
	bool canDoBlockDrops() {
		return MemoryUtil::callVirtualFunc<bool>(39, this);
	}
	float getSeenPercent(const Vec3<float>& center, const AABB& bb) {
		static auto addr = MemoryUtil::findSignature(Sigs::blockSource::getSeenPercent);
		if (!addr) return 0.f;
		using func_t = float(__fastcall*)(BlockSource*, const Vec3<float>&, const AABB&);
		static func_t func = reinterpret_cast<func_t>(addr);
		return func(this, center, bb);
	}
};
