#pragma once
#include "Actor.h"
#include "MobEffectInstance.h"

class Mob : public Actor {
public:
	CLASS_MEMBER(float, hurtDir, 0x3B0);
	CLASS_MEMBER(float, oTilt, 0x3B4);
	CLASS_MEMBER(float, tilt, 0x3B8);
	CLASS_MEMBER(float, attackAnim, 0x40C);
	CLASS_MEMBER(int, swingTime, 0x410);
	CLASS_MEMBER(float, movementSpeed, 0x430);
	CLASS_MEMBER(bool, swinging, 0x434);
	CLASS_MEMBER(bool, surfaceMob, 0x435);
	CLASS_MEMBER(bool, naturallySpawned, 0x436);
	CLASS_MEMBER(uint64_t, caravanHead, 0x440);
	CLASS_MEMBER(uint64_t, caravanTail, 0x448);
	CLASS_MEMBER(float, oAttackAnim, 0x450);
	CLASS_MEMBER(Vec3<int>, boundOrigin, 0x454);
	CLASS_MEMBER(uint64_t, targetCaptainId, 0x460);

	MobEffectInstance* getEffect(unsigned int effectId) {
		return nullptr;
	}
};
