#pragma once
#include <cstdint>
#include "../../Utils/MemoryUtil.h"
#include "../../Utils/Maths.h"
#include "BlockPos.h"
#include "../Signatures.h"
#include "../GameVersion.h"

class Player;
class ItemStack;
class Block;
class Actor;

class GameMode {
public:
	virtual ~GameMode();
	virtual bool startDestroyBlock(Vec3<int> const& pos, unsigned char face, bool& hasDestroyedBlock);
	virtual bool destroyBlock(Vec3<int> const& pos, unsigned char face);
	virtual bool continueDestroyBlock(Vec3<int> const& pos, unsigned char face, Vec3<float> const& mouseDelta, bool& hasDestroyedBlock);
	virtual void stopDestroyBlock4x();
	virtual bool startBuildBlock4x(Vec3<int> const& pos, unsigned char face);
	virtual bool buildBlock4x(Vec3<int> const& pos, unsigned char face, bool predict);
	virtual bool continueBuildBlock4x(Vec3<int> const& pos, unsigned char face);
	virtual void stopBuildBlock();
	virtual void tick();
	virtual float getPickRange4x();
	virtual void useItem4x(ItemStack& item);
	virtual void useItemAsAttack4x(ItemStack& item);
	virtual uint8_t* useItemOnRaw(uint8_t* result, ItemStack& item, BlockPos const& at, unsigned char face, Vec3<float> const& hit, Block const* targetBlock, bool isFirstEvent);
	virtual bool interact4x(Actor* entity, Vec3<int> const& pos);
	virtual bool attack(Actor* entity, Vec3<float> const& hitPosition);
	virtual void releaseUsingItem();
	virtual void setTrialMode(bool isTrialMode);
	virtual bool isInTrialMode();
	virtual void registerUpsellScreenCallback(uintptr_t callback);
	uint8_t useItemOn(ItemStack& item, BlockPos const& at, unsigned char face, Vec3<float> const& hit, Block const* targetBlock, bool isFirstEvent) {
		uint8_t result = 0;
		if (SdkVersion::is12651()) {
			MemoryUtil::callVirtualFunc<uint8_t*, uint8_t*, ItemStack&, const BlockPos&, unsigned char,
				const Vec3<float>&, uint8_t, const Block*, bool>(13, this, &result, item, at, face, hit,
				uint8_t{0}, targetBlock, isFirstEvent);
		} else {
			useItemOnRaw(&result, item, at, face, hit, targetBlock, isFirstEvent);
		}
		return result;
	}
	bool buildBlock(Vec3<int> const& pos, unsigned char face, bool predict) {
		if (!SdkVersion::is12651()) return buildBlock4x(pos, face, predict);
		return MemoryUtil::callVirtualFunc<bool, const Vec3<int>&, unsigned char, uint8_t, bool>(
			6, this, pos, face, uint8_t{0}, predict);
	}
	void useItem(ItemStack& item) {
		if (!SdkVersion::is12651()) { useItem4x(item); return; }
		MemoryUtil::callVirtualFunc<bool, ItemStack&, uint8_t>(11, this, item, uint8_t{0});
	}
	void stopDestroyBlock() {
		if (!SdkVersion::is12651()) { stopDestroyBlock4x(); return; }
		MemoryUtil::callVirtualFunc<void, const Vec3<int>&>(4, this, getdestroyBlockPos());
	}
	bool startBuildBlock(const Vec3<int>& pos, unsigned char face) {
		if (!SdkVersion::is12651()) return startBuildBlock4x(pos, face);
		MemoryUtil::callVirtualFunc<void, const Vec3<int>&, unsigned char, uint8_t>(5, this, pos, face, uint8_t{0});
		return true;
	}
	bool continueBuildBlock(const Vec3<int>& pos, unsigned char face) {
		if (!SdkVersion::is12651()) return continueBuildBlock4x(pos, face);
		MemoryUtil::callVirtualFunc<void, const Vec3<int>&, unsigned char, uint8_t>(7, this, pos, face, uint8_t{0});
		return true;
	}
	float getPickRange(uint32_t inputMode = 1) {
		if (!SdkVersion::is12651()) return getPickRange4x();
		return MemoryUtil::callVirtualFunc<float, const uint32_t&>(10, this, inputMode);
	}
	void useItemAsAttack(ItemStack& item, const Vec3<float>& aim = Vec3<float>(0.f, 0.f, 1.f)) {
		if (!SdkVersion::is12651()) { useItemAsAttack4x(item); return; }
		MemoryUtil::callVirtualFunc<bool, ItemStack&, const Vec3<float>&, uint8_t>(12, this, item, aim, uint8_t{0});
	}
	bool interact(Actor* entity, const Vec3<int>& pos) {
		if (!entity) return false;
		if (!SdkVersion::is12651()) return interact4x(entity, pos);
		const Vec3<float> hit(static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z));
		return MemoryUtil::callVirtualFunc<bool, Actor*, const Vec3<float>&, uint8_t>(14, this, entity, hit, uint8_t{0});
	}

	CLASS_MEMBER(Player*, player, 0x8);
	CLASS_MEMBER(Vec3<int>, destroyBlockPos, 0x10);
	CLASS_MEMBER(unsigned char, destroyBlockFace, 0x1C);
	CLASS_MEMBER(float, oldDestroyProgress, 0x20);
	CLASS_MEMBER(float, destroyProgress, 0x24);
	CLASS_MEMBER(double, lastDestroyTime, 0x28);
	CLASS_MEMBER(float, distanceTravelled, 0x30);
	CLASS_MEMBER(Vec3<float>, playerLastPosition, 0x34);
	CLASS_MEMBER(float, minPlayerSpeed, 0x7C);
	CLASS_MEMBER(int, continueBreakBlockCount, 0x80);
};
