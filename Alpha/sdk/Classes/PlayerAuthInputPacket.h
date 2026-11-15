#pragma once
#include "Packet.h"
#include "../../Utils/Maths.h"
#include <cstdint>

enum class AuthInputAction : uint64_t {
	NONE = 0,

	Ascend = 1ULL << 0,
	Descend = 1ULL << 1,
	NorthJump = 1ULL << 2,
	JumpDown = 1ULL << 3,
	SprintDown = 1ULL << 4,
	ChangeHeight = 1ULL << 5,
	Jumping = 1ULL << 6,
	AutoJumpingInWater = 1ULL << 7,
	Sneaking = 1ULL << 8,
	SneakDown = 1ULL << 9,
	Up = 1ULL << 10,
	Down = 1ULL << 11,
	Left = 1ULL << 12,
	Right = 1ULL << 13,
	UpLeft = 1ULL << 14,
	UpRight = 1ULL << 15,
	WantUp = 1ULL << 16,
	WantDown = 1ULL << 17,
	WantDownSlow = 1ULL << 18,
	WantUpSlow = 1ULL << 19,
	Sprinting = 1ULL << 20,
	AscendBlock = 1ULL << 21,
	DescendBlock = 1ULL << 22,
	SneakToggleDown = 1ULL << 23,
	PersistSneak = 1ULL << 24,
	StartSprinting = 1ULL << 25,
	StopSprinting = 1ULL << 26,
	StartSneaking = 1ULL << 27,
	StopSneaking = 1ULL << 28,
	StartSwimming = 1ULL << 29,
	StopSwimming = 1ULL << 30,
	StartJumping = 1ULL << 31,
	StartGliding = 1ULL << 32,
	StopGliding = 1ULL << 33,
	PerformItemInteraction = 1ULL << 34,
	PerformBlockActions = 1ULL << 35,
	PerformItemStackRequest = 1ULL << 36,
	HandledTeleport = 1ULL << 37,
	Emoting = 1ULL << 38,
	MissedSwing = 1ULL << 39,
	StartCrawling = 1ULL << 40,
	StopCrawling = 1ULL << 41,
	StartFlying = 1ULL << 42,
	StopFlying = 1ULL << 43,
	ReceivedServerData = 1ULL << 44,
	InClientPredictedInVehicle = 1ULL << 45,
	PaddlingLeft = 1ULL << 46,
	PaddlingRight = 1ULL << 47,
};

using InputData = AuthInputAction;

inline AuthInputAction operator|(AuthInputAction a, AuthInputAction b) {
	return (AuthInputAction)((uint64_t)a | (uint64_t)b);
}
inline AuthInputAction operator&(AuthInputAction a, AuthInputAction b) {
	return (AuthInputAction)((uint64_t)a & (uint64_t)b);
}
inline AuthInputAction operator~(AuthInputAction a) {
	return (AuthInputAction)(~(uint64_t)a);
}
inline void operator|=(AuthInputAction& a, AuthInputAction b) { a = a | b; }
inline void operator&=(AuthInputAction& a, AuthInputAction b) { a = a & b; }

class PlayerAuthInputPacket : public Packet {
public:
	Vec2<float> rotation;
	Vec3<float> position;
	float headYaw;
	Vec3<float> velocity;
	Vec2<float> vehicleRot;
	Vec2<float> analogMoveVector;
	Vec2<float> moveVector;
	Vec2<float> interactRotation;
	Vec3<float> gazeDir;
	Vec2<float> rawMoveVector;
	uint64_t mInputFlags[2];
	int mInputMode;
	int mPlayMode;
	int mNewInteractionModel;
	char pad_a4[4];
	uint64_t mClientTick;
	char mItemUseTransaction[8];
	char mItemStackRequest[8];
	char playerBlockActions[24];
	uint64_t mPredictedVehicle;

	inline bool hasInput(AuthInputAction input) const {
		return (mInputFlags[0] & (uint64_t)input) != 0;
	}

	inline void setInput(AuthInputAction input, bool toggle = true) {
		if (toggle)
			mInputFlags[0] |= (uint64_t)input;
		else
			mInputFlags[0] &= ~(uint64_t)input;
	}
};
