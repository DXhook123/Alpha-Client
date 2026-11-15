#pragma once
#include "../../sdk/Classes/Block.h"
#include "../../sdk/Classes/BlockPos.h"
#include "../../sdk/Classes/Actor.h"
#include "../../sdk/Core/ClientInstance.h"
#include "../../sdk/Game.h"
#include "../../Utils/Maths.h"

class BlockType;

namespace WorldUtil {
	Block* getBlock(const BlockPos& blockPos);
	BlockType* getBlockLegacy(const BlockPos& blockPos);
	bool isContainer(const BlockPos& blockPos);
	bool canBuildOn(const BlockPos& blockPos);
	bool isAirBlock(const BlockPos& blockPos);

	float distanceToPoint(const Vec3<float>& pos, const Vec3<float>& point);
	float distanceToEntity(const Vec3<float>& pos, Actor* entity);
	float distanceToBlock(const Vec3<float>& pos, const BlockPos& blockPos);

	bool rayCast(const Vec3<float>& start, const Vec3<float>& end, int ignoredBlockId = -1, const BlockPos* ignoredPos = nullptr);

	void beginSeenPercentTick();
	float getSeenPercent(const Vec3<float>& pos, const AABB& aabb);
	float getSeenPercent(const Vec3<float>& pos, Actor* actor);

	bool getMeleePointOnAABB(const Vec3<float>& pos, const AABB& aabb, Vec3<float>& outPoint,
		float minVisiblePercent = 0.02f);
	float getExposure(const Vec3<float>& pos, const AABB& aabb, int ignoredBlockId = -1);

	float sign(float f);
	float boundary(float start, float distance);
}
