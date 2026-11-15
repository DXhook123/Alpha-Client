#include "PlayerUtil.h"
#include "../../sdk/Classes/Container.h"
#include "../../sdk/Classes/PlayerInventory.h"
#include "../../sdk/Classes/ItemStack.h"
#include "../../sdk/Classes/Item.h"
#include <algorithm>
#include <vector>

bool PlayerUtil::canPlaceBlock(const BlockPos& blockPos, bool airPlace) {
	LocalPlayer* localPlayer = Game::getLocalPlayer();
	if (!localPlayer) return false;

	ClientInstance* ci = Game::getClientInstance();
	if (!ci) return false;
	BlockSource* region = ci->getRegion();
	if (!region) return false;

	Block* block = region->getBlock(blockPos);
	if (!block || !block->blockType) return false;

	bool canBeBuiltOver = !block->blockType->mSolid;
	if (!canBeBuiltOver) return false;

	if (airPlace) return true;

	BlockPos neighbors[6] = {
		{ blockPos.x,     blockPos.y - 1, blockPos.z     },
		{ blockPos.x,     blockPos.y + 1, blockPos.z     },
		{ blockPos.x,     blockPos.y,     blockPos.z - 1 },
		{ blockPos.x,     blockPos.y,     blockPos.z + 1 },
		{ blockPos.x - 1, blockPos.y,     blockPos.z     },
		{ blockPos.x + 1, blockPos.y,     blockPos.z     }
	};

	for (const BlockPos& nb : neighbors) {
		Block* b = region->getBlock(nb);
		if (!b || !b->blockType) continue;
		if (b->blockType->mSolid
			&& !WorldUtil::isContainer(nb)
			&& b->blockType->mID != 143) {
			return true;
		}
	}
	return false;
}
bool PlayerUtil::tryPlaceBlock(const BlockPos& blockPos, bool airPlace) {
	LocalPlayer* localPlayer = Game::getLocalPlayer();
	if (!localPlayer) return false;

	ClientInstance* ci = Game::getClientInstance();
	if (!ci) return false;
	BlockSource* region = ci->getRegion();
	if (!region) return false;

	Block* block = region->getBlock(blockPos);
	if (!block || !block->blockType) return false;

	bool canBeBuiltOver = !block->blockType->mSolid;
	if (!canBeBuiltOver) return false;

	BlockPos candidates[6] = {
		{ blockPos.x,     blockPos.y - 1, blockPos.z     },
		{ blockPos.x,     blockPos.y + 1, blockPos.z     },
		{ blockPos.x,     blockPos.y,     blockPos.z - 1 },
		{ blockPos.x,     blockPos.y,     blockPos.z + 1 },
		{ blockPos.x - 1, blockPos.y,     blockPos.z     },
		{ blockPos.x + 1, blockPos.y,     blockPos.z     }
	};

	bool foundCandidate = false;
	uint8_t face = 0;
	BlockPos blockToPlace = blockPos;

	for (uint8_t i = 0; i < 6; i++) {
		Block* b = region->getBlock(candidates[i]);
		if (!b || !b->blockType) continue;
		if (b->blockType->mSolid
			&& !WorldUtil::isContainer(candidates[i])
			&& b->blockType->mID != 143) {
			foundCandidate = true;
			blockToPlace = candidates[i];
			face = i;
			break;
		}
	}
	if (airPlace && !foundCandidate) {
		foundCandidate = true;
		blockToPlace = blockPos;
		face = 0;
	}

	if (!foundCandidate) return false;

	static const uint8_t kCandidateToMcFace[6] = { 1, 0, 3, 2, 5, 4 };
	uint8_t mcFace = kCandidateToMcFace[face];

	GameMode* gm = localPlayer->gameMode;
	if (!gm) return false;
	return gm->buildBlock(Vec3<int>(blockToPlace.x, blockToPlace.y, blockToPlace.z), mcFace, false);
}

bool PlayerUtil::tryPlaceBlockSupport(const BlockPos& pos, int proximity, bool airPlace) {
	LocalPlayer* localPlayer = Game::getLocalPlayer();
	if (!localPlayer) return false;

	if (tryPlaceBlock(pos, airPlace)) return true;

	std::vector<BlockPos> checkList;
	checkList.reserve((2 * proximity + 1) * (2 * proximity + 1) * (2 * proximity + 1));
	for (int x = -proximity; x <= proximity; x++) {
		for (int y = -proximity; y <= proximity; y++) {
			for (int z = -proximity; z <= proximity; z++) {
				checkList.emplace_back(pos.x + x, pos.y + y, pos.z + z);
			}
		}
	}
	std::sort(checkList.begin(), checkList.end(), [&](const BlockPos& a, const BlockPos& b) {
		Vec3<float> posF((float)pos.x, (float)pos.y, (float)pos.z);
		Vec3<float> aF((float)a.x, (float)a.y, (float)a.z);
		Vec3<float> bF((float)b.x, (float)b.y, (float)b.z);
		return posF.dist(aF) < posF.dist(bF);
	});
	auto it = std::find_if(checkList.begin(), checkList.end(), [](const BlockPos& p) {
		return !WorldUtil::canBuildOn(p);
	});
	if (it == checkList.end()) return false;
	BlockPos nearestBlock = *it;
	std::vector<BlockPos> offsets;
	offsets.reserve(27);
	for (int x = -1; x <= 1; x++) {
		for (int y = -1; y <= 1; y++) {
			for (int z = -1; z <= 1; z++) {
				offsets.emplace_back(nearestBlock.x + x, nearestBlock.y + y, nearestBlock.z + z);
			}
		}
	}
	std::sort(offsets.begin(), offsets.end(), [&](const BlockPos& a, const BlockPos& b) {
		Vec3<float> posF((float)pos.x, (float)pos.y, (float)pos.z);
		Vec3<float> aF((float)a.x, (float)a.y, (float)a.z);
		Vec3<float> bF((float)b.x, (float)b.y, (float)b.z);
		return posF.dist(aF) < posF.dist(bF);
	});

	for (const BlockPos& off : offsets) {
		if (canPlaceBlock(off, airPlace)) {
			if (tryPlaceBlock(off, airPlace)) {
				localPlayer->swing();
				break;
			}
		}
	}
	return true;
}
