#include "WorldUtil.h"
#include <cstdint>
#include <limits>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
	struct MeleePointKey {
		BlockSource* region = nullptr;
		Vec3<float> source{};
		AABB aabb{};
		float minVisiblePercent = 0.f;

		bool operator==(const MeleePointKey& other) const {
			return region == other.region
				&& minVisiblePercent == other.minVisiblePercent
				&& source.x == other.source.x && source.y == other.source.y && source.z == other.source.z
				&& aabb.lower.x == other.aabb.lower.x && aabb.lower.y == other.aabb.lower.y && aabb.lower.z == other.aabb.lower.z
				&& aabb.upper.x == other.aabb.upper.x && aabb.upper.y == other.aabb.upper.y && aabb.upper.z == other.aabb.upper.z;
		}
	};

	struct MeleePointKeyHash {
		size_t operator()(const MeleePointKey& key) const {
			size_t hash = std::hash<const void*>{}(key.region);
			auto combine = [&hash](float value) {
				hash ^= std::hash<float>{}(value) + (hash << 6) + (hash >> 2);
			};
			combine(key.source.x); combine(key.source.y); combine(key.source.z);
			combine(key.aabb.lower.x); combine(key.aabb.lower.y); combine(key.aabb.lower.z);
			combine(key.aabb.upper.x); combine(key.aabb.upper.y); combine(key.aabb.upper.z);
			combine(key.minVisiblePercent);
			return hash;
		}
	};

	struct MeleePointResult {
		bool visible = false;
		Vec3<float> point{};
	};

	std::unordered_map<MeleePointKey, MeleePointResult, MeleePointKeyHash> gMeleePointCache;
	std::mutex gMeleePointCacheMutex;
	uint64_t gMeleePointGeneration = 0;
}

Block* WorldUtil::getBlock(const BlockPos& blockPos) {
	ClientInstance* ci = Game::getClientInstance();
	if (!ci) return nullptr;
	BlockSource* region = ci->getRegion();
	if (!region) return nullptr;
	return region->getBlock(blockPos);
}

BlockType* WorldUtil::getBlockLegacy(const BlockPos& blockPos) {
	Block* block = getBlock(blockPos);
	if (!block) return nullptr;
	return block->blockType;
}

bool WorldUtil::isContainer(const BlockPos& blockPos) {
	BlockType* legacy = getBlockLegacy(blockPos);
	if (!legacy) return false;
	return legacy->mID == 130 || legacy->mID == 54 || legacy->mID == 146 || legacy->mID == 145;
}

bool WorldUtil::isAirBlock(const BlockPos& blockPos) {
	BlockType* legacy = getBlockLegacy(blockPos);
	if (!legacy) return true;
	return legacy->mID == 0;
}

bool WorldUtil::canBuildOn(const BlockPos& blockPos) {
	ClientInstance* ci = Game::getClientInstance();
	if (!ci) return false;
	BlockSource* region = ci->getRegion();
	if (!region) return false;
	Block* block = getBlock(blockPos);
	if (!block || !block->blockType) return false;
	return !block->blockType->mSolid;
}
float WorldUtil::distanceToPoint(const Vec3<float>& pos, const Vec3<float>& point) {
	return pos.dist(point);
}

float WorldUtil::distanceToEntity(const Vec3<float>& pos, Actor* entity) {
	if (entity == nullptr) return 0.f;
	return distanceToPoint(pos, entity->getPos());
}

float WorldUtil::distanceToBlock(const Vec3<float>& pos, const BlockPos& blockPos) {
	Vec3<float> blockCenter(
		(float)blockPos.x + 0.5f,
		(float)blockPos.y + 0.5f,
		(float)blockPos.z + 0.5f
	);
	return distanceToPoint(pos, blockCenter);
}

float WorldUtil::sign(float f) {
	if (f > 0) return 1.f;
	if (f < 0) return -1.f;
	return 0.f;
}
float WorldUtil::boundary(float start, float distance) {
	if (distance == 0) return std::numeric_limits<float>::infinity();
	float s = start;
	float d = distance;
	if (d < 0) {
		s = -s;
		d = -d;
		if (std::floor(s) == s) return 0.f;
	}
	return (1.f - (s - std::floor(s))) / d;
}

bool WorldUtil::rayCast(const Vec3<float>& start, const Vec3<float>& end, int ignoredBlockId, const BlockPos* ignoredPos) {
	Vec3<float> delta = end.sub(start);
	float radius = delta.magnitude();
	if (radius < 1e-6f) return false;
	ClientInstance* ci = Game::getClientInstance();
	BlockSource* region = ci ? ci->getRegion() : nullptr;
	if (!region) return false;

	Vec3<float> direction(delta.x / radius, delta.y / radius, delta.z / radius);

	int currentX = (int)std::floor(start.x);
	int currentY = (int)std::floor(start.y);
	int currentZ = (int)std::floor(start.z);

	const int stepX = (int)sign(direction.x);
	const int stepY = (int)sign(direction.y);
	const int stepZ = (int)sign(direction.z);

	float tMaxX = boundary(start.x, direction.x);
	float tMaxY = boundary(start.y, direction.y);
	float tMaxZ = boundary(start.z, direction.z);

	const float infinity = std::numeric_limits<float>::infinity();
	const float tDeltaX = (direction.x == 0.f) ? infinity : std::abs(1.f / direction.x);
	const float tDeltaY = (direction.y == 0.f) ? infinity : std::abs(1.f / direction.y);
	const float tDeltaZ = (direction.z == 0.f) ? infinity : std::abs(1.f / direction.z);

	bool firstBlock = true;

	while (true) {
		BlockPos curBlock(currentX, currentY, currentZ);
		Block* block = region->getBlock(curBlock);
		if (block && block->blockType) {
			if (block->blockType->mSolid && !(ignoredPos && curBlock == *ignoredPos)) {
				if (firstBlock) {
					if (ignoredBlockId < 0 || block->blockType->mID != (uint16_t)ignoredBlockId) {
						return true;
					}
				}
				else {
					return true;
				}
			}
		}

		const float nextT = (std::min)((std::min)(tMaxX, tMaxY), tMaxZ);
		if (!std::isfinite(nextT) || nextT > radius) break;
		constexpr float tieEpsilon = 1e-5f;
		if (tMaxX <= nextT + tieEpsilon) {
			currentX += stepX;
			tMaxX += tDeltaX;
		}
		if (tMaxY <= nextT + tieEpsilon) {
			currentY += stepY;
			tMaxY += tDeltaY;
		}
		if (tMaxZ <= nextT + tieEpsilon) {
			currentZ += stepZ;
			tMaxZ += tDeltaZ;
		}

		firstBlock = false;
	}
	return false;
}

void WorldUtil::beginSeenPercentTick() {
	std::lock_guard<std::mutex> lock(gMeleePointCacheMutex);
	gMeleePointCache.clear();
	++gMeleePointGeneration;
}

float WorldUtil::getSeenPercent(const Vec3<float>& pos, const AABB& aabb) {
	ClientInstance* ci = Game::getClientInstance();
	if (!ci) return 0.f;
	BlockSource* region = ci->getRegion();
	if (!region) return 0.f;
	const float seenPercent = region->getSeenPercent(pos, aabb);
	return seenPercent > 1.f ? 1.f : seenPercent;
}

float WorldUtil::getSeenPercent(const Vec3<float>& pos, Actor* actor) {
	if (!actor) return 0.f;
	AABBShapeComponent* aabbComp = actor->aabbShape;
	if (!aabbComp) return 0.f;
	return getSeenPercent(pos, aabbComp->mAABB);
}

namespace {
	constexpr float kMeleeCenterOffsets[] = { 0.5f };
	constexpr float kMeleeMidOffsets[] = { 0.25f, 0.75f };
	constexpr float kMeleeEdgeOffsets[] = { 0.03f, 0.97f };
	Vec3<float> meleeSamplePoint(const AABB& box, float fx, float fy, float fz, float inset) {
		auto axis = [inset](float lower, float upper, float factor) {
			const float value = lower + (upper - lower) * factor;
			if (value < lower + inset) return lower + inset;
			if (value > upper - inset) return upper - inset;
			return value;
		};
		return Vec3<float>(
			axis(box.lower.x, box.upper.x, fx),
			axis(box.lower.y, box.upper.y, fy),
			axis(box.lower.z, box.upper.z, fz));
	}

	Vec3<float> meleeFacePoint(const AABB& box, int axisIndex, bool upper, float fa, float fb) {
		auto lerpAxis = [](float lower, float upper, float factor) {
			return lower + (upper - lower) * factor;
		};
		const float fixed = upper ? 1.f : 0.f;
		switch (axisIndex) {
		case 0: {
			const float x = lerpAxis(box.lower.x, box.upper.x, fixed);
			return Vec3<float>(x,
				lerpAxis(box.lower.y, box.upper.y, fa),
				lerpAxis(box.lower.z, box.upper.z, fb));
		}
		case 1: {
			const float y = lerpAxis(box.lower.y, box.upper.y, fixed);
			return Vec3<float>(lerpAxis(box.lower.x, box.upper.x, fa),
				y,
				lerpAxis(box.lower.z, box.upper.z, fb));
		}
		default: {
			const float z = lerpAxis(box.lower.z, box.upper.z, fixed);
			return Vec3<float>(lerpAxis(box.lower.x, box.upper.x, fa),
				lerpAxis(box.lower.y, box.upper.y, fb),
				z);
		}
		}
	}
	std::vector<Vec3<float>> meleeBuildCandidates(const Vec3<float>& pos, const AABB& box, float inset) {
		std::vector<std::pair<float, Vec3<float>>> ordered;
		ordered.reserve(71);
		int order = 0;
		for (int tier = 0; tier < 3; tier++) {
			const float* offsets = nullptr;
			int count = 0;
			switch (tier) {
			case 0: offsets = kMeleeCenterOffsets; count = 1; break;
			case 1: offsets = kMeleeMidOffsets; count = 2; break;
			default: offsets = kMeleeEdgeOffsets; count = 2; break;
			}
			for (int ix = 0; ix < count; ix++) {
				for (int iy = 0; iy < count; iy++) {
					for (int iz = 0; iz < count; iz++) {
						const Vec3<float> point = meleeSamplePoint(box, offsets[ix], offsets[iy], offsets[iz], inset);
						ordered.push_back({ (float)order * 100000.f + point.dist(pos), point });
					}
				}
			}
			order++;
		}

		const int faceOrder = order;
		for (int axisIndex = 0; axisIndex < 3; axisIndex++) {
			for (int side = 0; side < 2; side++) {
				for (float fa : { 0.5f, 0.25f, 0.75f }) {
					for (float fb : { 0.5f, 0.25f, 0.75f }) {
						const Vec3<float> point = meleeFacePoint(box, axisIndex, side == 1, fa, fb);
						ordered.push_back({ (float)faceOrder * 100000.f + point.dist(pos), point });
					}
				}
			}
		}

		std::sort(ordered.begin(), ordered.end(),
			[](const std::pair<float, Vec3<float>>& a, const std::pair<float, Vec3<float>>& b) {
				return a.first < b.first;
			});

		std::vector<Vec3<float>> out;
		out.reserve(ordered.size());
		for (const auto& entry : ordered) out.push_back(entry.second);
		return out;
	}
}

bool WorldUtil::getMeleePointOnAABB(const Vec3<float>& pos, const AABB& aabb, Vec3<float>& outPoint,
	float minVisiblePercent) {
	outPoint = aabb.getCenter();
	ClientInstance* ci = Game::getClientInstance();
	if (!ci) return false;
	BlockSource* region = ci->getRegion();
	if (!region) return false;

	const Vec3<float>& min = aabb.lower;
	const Vec3<float>& max = aabb.upper;
	if (!std::isfinite(min.x) || !std::isfinite(min.y) || !std::isfinite(min.z)
		|| !std::isfinite(max.x) || !std::isfinite(max.y) || !std::isfinite(max.z)
		|| max.x < min.x || max.y < min.y || max.z < min.z) {
		return false;
	}

	const MeleePointKey key{ region, pos, aabb, minVisiblePercent };
	uint64_t generation;
	{
		std::lock_guard<std::mutex> lock(gMeleePointCacheMutex);
		generation = gMeleePointGeneration;
		auto cached = gMeleePointCache.find(key);
		if (cached != gMeleePointCache.end()) {
			outPoint = cached->second.point;
			return cached->second.visible;
		}
	}

	float inset = 0.05f;
	const float shortest = (std::min)((std::min)(max.x - min.x, max.y - min.y), max.z - min.z);
	if (shortest > 0.f) inset = (std::min)(inset, shortest * 0.25f);

	bool visible = false;
	const Vec3<float> center = meleeSamplePoint(aabb, 0.5f, 0.5f, 0.5f, inset);
	if (!rayCast(pos, center)) {
		visible = minVisiblePercent <= 0.f || getSeenPercent(pos, aabb) >= minVisiblePercent;
		if (visible) outPoint = center;
	}
	else {
		const auto candidates = meleeBuildCandidates(pos, aabb, inset);
		for (std::size_t i = 1; i < candidates.size(); ++i) {
			if (rayCast(pos, candidates[i])) continue;
			if (minVisiblePercent > 0.f && getSeenPercent(pos, aabb) < minVisiblePercent) break;
			outPoint = candidates[i];
			visible = true;
			break;
		}
	}

	{
		std::lock_guard<std::mutex> lock(gMeleePointCacheMutex);
		if (generation == gMeleePointGeneration) {
			if (gMeleePointCache.size() >= 4096) gMeleePointCache.clear();
			gMeleePointCache.emplace(key, MeleePointResult{ visible, outPoint });
		}
	}
	return visible;
}

float WorldUtil::getExposure(const Vec3<float>& pos, const AABB& aabb, int ignoredBlockId) {
	BlockPos blockPos((int)pos.x, (int)pos.y, (int)pos.z);
	Block* block = getBlock(blockPos);
	if (!block || !block->blockType) return 0.f;
	if ((int)block->blockType->mID != ignoredBlockId) {
		return getSeenPercent(pos, aabb);
	}
	if (pos.y > aabb.lower.y) {
		BlockPos below((int)pos.x, (int)pos.y - 1, (int)pos.z);
		Block* belowBlock = getBlock(below);
		if (belowBlock && belowBlock->blockType && belowBlock->blockType->mSolid) {
			return 0.f;
		}
	}
	return 0.f;
}
