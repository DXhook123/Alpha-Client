#include "FeetTrap.h"

#include "../Settings/SliderSetting.h"
#include "../Settings/BoolSetting.h"
#include "sdk/Game.h"
#include "sdk/Classes/ActorFlags.h"
#include "sdk/Classes/LocalPlayer.h"
#include "sdk/Classes/Level.h"
#include "sdk/Classes/GameMode.h"
#include "sdk/Classes/BlockSource.h"
#include "sdk/Classes/Block.h"
#include "sdk/Classes/BlockType.h"
#include "sdk/Classes/PlayerInventory.h"
#include "sdk/Classes/Container.h"
#include "sdk/Classes/Item.h"
#include "sdk/Classes/ItemStackBase.h"
#include "sdk/Classes/PlayerAuthInputPacket.h"
#include "sdk/Classes/ActorType.h"
#include "sdk/Components/AABBShapeComponent.h"
#include "sdk/Components/ActorDataFlagComponent.h"
#include "sdk/Components/StateVectorComponent.h"
#include "sdk/Components/ActorTypeComponent.h"
#include "sdk/Components/ActorHeadRotationComponent.h"
#include "sdk/Components/MobBodyRotationComponent.h"
#include "Utils/Minecraft/WorldUtil.h"
#include "Utils/ColorUtil.h"
#include "Renderer/DrawUtil.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace {
	struct BlockPosHash {
		size_t operator()(const BlockPos& p) const noexcept {
			size_t h = (size_t)(uint32_t)p.x;
			h = h * 1315423911u ^ (size_t)(uint32_t)p.y;
			h = h * 2654435761u ^ (size_t)(uint32_t)p.z;
			return h;
		}
	};

	using BlockPosSet = std::unordered_set<BlockPos, BlockPosHash>;
	using CrystalObstructMap = std::unordered_multimap<BlockPos, Actor*, BlockPosHash>;

	AABB getHardcodedAABB(const AABB& bb) {
		static constexpr float WIDTH = 0.6f;
		float xerr = (WIDTH - (bb.upper.x - bb.lower.x)) * 0.5f;
		float zerr = (WIDTH - (bb.upper.z - bb.lower.z)) * 0.5f;
		return AABB(
			Vec3<float>(bb.lower.x - xerr, bb.lower.y, bb.lower.z - zerr),
			Vec3<float>(bb.upper.x + xerr, bb.upper.y, bb.upper.z + zerr)
		);
	}

	template <class Fn>
	void forEachBlockInAABB(const AABB& bb, Fn&& fn) {
		const Vec3<float> hiF = bb.upper.sub(0.001f, 0.001f, 0.001f);
		const int lox = (int)std::floor(bb.lower.x);
		const int loy = (int)std::floor(bb.lower.y);
		const int loz = (int)std::floor(bb.lower.z);
		const int hix = (int)std::floor(hiF.x);
		const int hiy = (int)std::floor(hiF.y);
		const int hiz = (int)std::floor(hiF.z);
		for (int x = lox; x <= hix; x++)
			for (int y = loy; y <= hiy; y++)
				for (int z = loz; z <= hiz; z++)
					fn(BlockPos(x, y, z));
	}

	int axisDist(const BlockPos& a, const BlockPos& b) {
		int dx = std::abs(a.x - b.x);
		int dy = std::abs(a.y - b.y);
		int dz = std::abs(a.z - b.z);
		int m = dx;
		if (dy > m) m = dy;
		if (dz > m) m = dz;
		return m;
	}

	void findObstructedBlocks(Level* level, const BlockPos& center, int range,
		BlockPosSet& obstructed, CrystalObstructMap& crystals)
	{
		LocalPlayer* lp = Game::getLocalPlayer();
		if (!level) return;
		for (Actor* ent : level->getRuntimeActorList()) {
			if (!ent || !ent->aabbShape) continue;
			auto* tc = ent->getActorTypeComponent();
			if (!tc) continue;
			const AABB bb = (ent == lp) ? getHardcodedAABB(ent->aabbShape->mAABB)
				: ent->aabbShape->mAABB;
			forEachBlockInAABB(bb, [&](const BlockPos& entBlock) {
				if (axisDist(entBlock, center) > range) return;
				if (tc->mType == ActorType::ItemEntity) return;
				if (tc->mType == ActorType::EnderCrystal) crystals.emplace(entBlock, ent);
				else              obstructed.insert(entBlock);
				});
		}
	}

	void addBlock(const BlockPos& block,
		std::vector<BlockPos>& toPlace,
		std::vector<Actor*>& toBreak,
		const CrystalObstructMap& map)
	{
		auto range = map.equal_range(block);
		for (auto it = range.first; it != range.second; ++it) {
			Actor* a = it->second;
			if (!a || !a->isAlive()) continue;
			if (std::find(toBreak.begin(), toBreak.end(), a) == toBreak.end())
				toBreak.push_back(a);
		}
		if (std::find(toPlace.begin(), toPlace.end(), block) == toPlace.end())
			toPlace.push_back(block);
	}

	bool isEpicBlock(ItemStack* item) {
		if (!item || !item->mValid) return false;
		Item* it = item->getItem();
		if (!it) return false;
		return it->mId == 49 || it->mId == 7;
	}

	std::optional<int> findObby() {
		LocalPlayer* lp = Game::getLocalPlayer();
		if (!lp || !lp->playerInventory || !lp->playerInventory->container) return {};
		for (int n = 0; n < 9; n++) {
			ItemStack* stack = lp->playerInventory->container->getItem(n);
			if (isEpicBlock(stack)) return n;
		}
		return {};
	}

	const std::array<BlockPos, 4>& offsets() {
		static const std::array<BlockPos, 4> v = {
			BlockPos{ 1, 0,  0 },
			BlockPos{ 0, 0,  1 },
			BlockPos{-1, 0,  0 },
			BlockPos{ 0, 0, -1 }
		};
		return v;
	}

	struct FaceMap { BlockPos off; uint8_t face; };
	const std::array<FaceMap, 6>& faceSides() {
		static const std::array<FaceMap, 6> v = {
			FaceMap{ BlockPos(0,  1,  0), 0 },
			FaceMap{ BlockPos(0, -1,  0), 1 },
			FaceMap{ BlockPos(0,  0,  1), 2 },
			FaceMap{ BlockPos(0,  0, -1), 3 },
			FaceMap{ BlockPos(1,  0,  0), 4 },
			FaceMap{ BlockPos(-1, 0,  0), 5 },
		};
		return v;
	}

	inline bool blockIsSolid(const BlockPos& p) {
		Block* b = WorldUtil::getBlock(p);
		return b && b->blockType && b->blockType->mSolid;
	}
	inline bool blockIsReplaceable(const BlockPos& p) {
		Block* b = WorldUtil::getBlock(p);
		if (!b || !b->blockType) return false;
		return !b->blockType->mSolid;
	}
	inline int blockId(const BlockPos& p) {
		Block* b = WorldUtil::getBlock(p);
		return (b && b->blockType) ? (int)b->blockType->mID : -1;
	}

	bool isInteractiveOrCrafting(const BlockPos& p) {
		if (WorldUtil::isContainer(p)) return true;
		switch (blockId(p)) {
		case 23: case 25: case 54: case 58:
		case 61: case 62: case 84: case 116:
		case 117: case 118: case 130: case 138:
		case 145: case 146: case 154: case 158:
		case 200: case 218:
		case 219: case 220: case 221: case 222: case 223:
		case 224: case 225: case 226: case 227: case 228:
		case 229: case 230: case 231: case 232: case 233:
		case 379: case 449: case 450: case 451: case 452:
		case 453: case 454: case 455: case 458: case 461: case 465:
			return true;
		}
		return false;
	}

	bool findPlaceFace(const BlockPos& block, bool sneaking,
		BlockPos& placeOn, uint8_t& face) {
		for (const FaceMap& side : faceSides()) {
			const BlockPos support = block.add(side.off);
			if (!blockIsSolid(support)) continue;
			if (!sneaking && isInteractiveOrCrafting(support)) continue;
			placeOn = support;
			face = side.face;
			return true;
		}
		return false;
	}

	bool rotationMatches(const Vec2<float>& current, const Vec2<float>& wanted) {
		const float pitchDiff = std::fabs(current.x - wanted.x);
		float yawDiff = std::fmod(std::fabs(current.y - wanted.y), 360.0f);
		if (yawDiff > 180.0f) yawDiff = 360.0f - yawDiff;
		return pitchDiff <= 0.1f && yawDiff <= 0.1f;
	}

	Actor* firstAlive(const std::vector<Actor*>& actors) {
		for (Actor* a : actors) {
			if (a && a->isAlive()) return a;
		}
		return nullptr;
	}

	Vec3<float> blockCenter(const BlockPos& block) {
		return Vec3<float>((float)block.x + 0.5f, (float)block.y + 0.5f, (float)block.z + 0.5f);
	}

	bool isUsingItem(LocalPlayer* lp) {
		auto* flags = lp->getDataFlagComponent();
		return flags && (flags->getStatusFlag(ActorFlags::Eating)
			|| flags->getStatusFlag(ActorFlags::UsingItem));
	}
}

FeetTrap::FeetTrap() : Module("FeetTrap", "Genius surround", Category::COMBAT) {
	registerSetting(new SliderSetting<int>("Distance", "Hole search distance", &distance, distance, 1, 7));
	registerSetting(new SliderSetting<int>("Delay", "Milliseconds between placement batches", &delay, delay, 0, 500));
	registerSetting(new SliderSetting<int>("Blocks", "Blocks placed per batch", &blocks, blocks, 1, 10));
	registerSetting(new BoolSetting("Swing", "Swing arm on place", &swing, swing));
	registerSetting(new BoolSetting("JumpDisable", "Disable module on jump", &jumpDisable, jumpDisable));
	registerSetting(new BoolSetting("PauseOnUse", "Pause while eating or using an item", &pauseOnUse, pauseOnUse));
	registerSetting(new BoolSetting("AttackCrystal", "Break crystals obstructing the trap", &attackCrystal, attackCrystal));
	registerSetting(new BoolSetting("Render", "Draw placement targets", &render, render));
}

void FeetTrap::onEnable() {
	lastPlaceTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(delay + 1000);
	rotate.reset();
	curRotate = Vec2<float>(0.f, 0.f);
	sneaking = false;
}

void FeetTrap::onDisable() {
	toPlace.clear();
	toBreak.clear();
	rotate.reset();
}

void FeetTrap::onNormalTick(LocalPlayer* lp) {
	if (!lp || !lp->aabbShape || !lp->gameMode || !lp->playerInventory) return;

	if (jumpDisable) {
		const float vy = (lp->stateVector ? lp->stateVector->mPosDelta.y : 0.f);
		if (vy >= 0.20f) {
			setEnabled(false);
			return;
		}
	}

	if (pauseOnUse && isUsingItem(lp)) {
		toPlace.clear();
		toBreak.clear();
		rotate.reset();
		return;
	}

	auto slot = findObby();
	if (!slot) {
		toPlace.clear();
		toBreak.clear();
		rotate.reset();
		return;
	}

	findBlocks();

	Actor* breakTarget = attackCrystal ? firstAlive(toBreak) : nullptr;
	if (breakTarget)
		rotate = lp->getEyePos().CalcAngle(breakTarget->getAABB().getCenter());
	else
		rotate.reset();

	const auto now = std::chrono::steady_clock::now();
	if (toPlace.empty()
		|| std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPlaceTime).count() < delay)
		return;
	lastPlaceTime = now;

	const uint8_t prevSlot = lp->playerInventory->mSelected;
	lp->playerInventory->mSelected = (uint8_t)slot.value();

	if (breakTarget) {
		if (!rotate.has_value() || !rotationMatches(curRotate, rotate.value())) {
			lp->playerInventory->mSelected = prevSlot;
			return;
		}
		lp->gameMode->attack(breakTarget, breakTarget->getAABB().getCenter());
		if (swing) lp->swing(ActorSwingSource::Attack);
	}

	int placed = 0;
	for (const BlockPos& block : toPlace) {
		if (blockIsSolid(block)) continue;

		BlockPos support;
		uint8_t face = 0;
		if (!findPlaceFace(block, sneaking, support, face)) continue;

		lp->gameMode->buildBlock(support, face, false);
		if (swing) lp->swing();
		if (++placed >= blocks) break;
	}

	lp->playerInventory->mSelected = prevSlot;
}

void FeetTrap::onUpdateRotation(LocalPlayer* lp) {
	if (!lp || !rotate.has_value()) return;
	ActorHeadRotationComponent* head = lp->getHeadRotationComponent();
	MobBodyRotationComponent* body = lp->getBodyRotationComponent();
	if (head) head->mHeadRot = rotate->y;
	if (body) body->mYBodyRot = rotate->y;
}

void FeetTrap::onSendPacket(Packet* packet, bool& cancel) {
	(void)cancel;
	if (!packet) return;
	if (packet->getId() != PacketID::PlayerAuthInput) return;

	PlayerAuthInputPacket* p = (PlayerAuthInputPacket*)packet;
	if (rotate.has_value()) {
		p->rotation.x = rotate.value().x;
		p->rotation.y = rotate.value().y;
		p->headYaw = rotate.value().y;
	}
	curRotate = Vec2<float>(p->rotation.x, p->rotation.y);
	sneaking = p->hasInput(AuthInputAction::Sneaking);
}

void FeetTrap::findBlocks() {
	toPlace.clear();
	toBreak.clear();

	LocalPlayer* lp = Game::getLocalPlayer();
	if (!lp || !lp->aabbShape) return;
	Level* level = lp->level;
	if (!level) return;
	ClientInstance* ci = Game::getClientInstance();
	if (!ci || !ci->getRegion()) return;

	BlockPosSet      obstructed, visited;
	CrystalObstructMap crystalObstructed;

	const BlockPos center = lp->getPos().CastTo<int>().sub(0, 1, 0);
	findObstructedBlocks(level, center, distance + 2, obstructed, crystalObstructed);

	std::queue<std::pair<BlockPos, int>> blockQueue;
	AABB myBB = getHardcodedAABB(lp->aabbShape->mAABB);

	{
		const Vec3<float> hiBottom(
			myBB.upper.x - 0.001f,
			myBB.lower.y + 0.5f,
			myBB.upper.z - 0.001f);
		const int lox = (int)std::floor(myBB.lower.x);
		const int loy = (int)std::floor(myBB.lower.y);
		const int loz = (int)std::floor(myBB.lower.z);
		const int hix = (int)std::floor(hiBottom.x);
		const int hiy = (int)std::floor(hiBottom.y);
		const int hiz = (int)std::floor(hiBottom.z);

		for (int x = lox; x <= hix; x++) {
			for (int y = loy; y <= hiy; y++) {
				for (int z = loz; z <= hiz; z++) {
					const BlockPos block(x, y, z);
					if (y == loy && blockIsReplaceable(block.sub(0, 1, 0)))
						addBlock(block.sub(0, 1, 0), toPlace, toBreak, crystalObstructed);
					blockQueue.emplace(block, 0);
				}
			}
		}
	}

	while (!blockQueue.empty()) {
		auto [block, dist] = blockQueue.front();
		blockQueue.pop();

		if (visited.find(block) != visited.end()) continue;
		visited.insert(block);

		bool canPlace = true;
		do {
			if (obstructed.find(block) != obstructed.end()) { canPlace = false; break; }
			const BlockPos blockDown = block.sub(0, 1, 0);
			if (blockIsSolid(blockDown)) break;
			if (obstructed.find(blockDown) != obstructed.end()) canPlace = false;
		} while (false);

		if (!canPlace) {
			if (dist == distance) continue;

			const int id = blockId(block);
			if (id == 49 || id == 7) continue;

			for (const BlockPos& off : offsets()) {
				const BlockPos next = block.add(off);
				if (visited.find(next) != visited.end()) continue;
				blockQueue.emplace(next, dist + 1);
			}
		}
		else {
			if (!blockIsSolid(block.sub(0, 1, 0)))
				addBlock(block.sub(0, 1, 0), toPlace, toBreak, crystalObstructed);
			addBlock(block, toPlace, toBreak, crystalObstructed);
		}
	}
}

void FeetTrap::onLevelRender() {
	if (!render) return;
	const UIColor fillColor(255, 255, 255, 60);
	const UIColor lineColor(255, 255, 255, 60);
	for (const BlockPos& block : toPlace) {
		AABB aabb;
		aabb.lower = Vec3<float>((float)block.x, (float)block.y, (float)block.z);
		aabb.upper = Vec3<float>((float)block.x + 1.f, (float)block.y + 1.f, (float)block.z + 1.f);
		DrawUtil::drawBox3dFilled(aabb, fillColor, lineColor);
	}
}
