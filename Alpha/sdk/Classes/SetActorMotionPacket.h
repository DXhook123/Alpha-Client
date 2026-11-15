#pragma once

#include "../../Utils/MemoryUtil.h"
#include "../Signatures.h"
#include "Packet.h"
#include "ActorRuntimeID.h"
class SetActorMotionPacket : public Packet {
public:
	static const PacketID ID = PacketID::SetActorMotion;

	CLASS_MEMBER(ActorRuntimeID, mRuntimeId, 0x30);
	CLASS_MEMBER(Vec3<float>, mMotion, 0x38);
	CLASS_MEMBER(uint64_t, mTick, 0x44);

	SetActorMotionPacket() = default;
};
