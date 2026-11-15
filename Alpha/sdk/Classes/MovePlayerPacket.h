#pragma once

#include "../../Utils/MemoryUtil.h"
#include "../Signatures.h"
#include "Packet.h"
#include "ActorRuntimeID.h"
#include "../../Utils/Maths.h"

class MovePlayerPacket : public Packet {
public:
	static const PacketID ID = PacketID::MovePlayerPacket;

	CLASS_MEMBER(ActorRuntimeID, mPlayerID, 0x30);
	CLASS_MEMBER(Vec3<float>, mPos, 0x38);
	CLASS_MEMBER(Vec2<float>, mRot, 0x44);
	CLASS_MEMBER(float, mYHeadRot, 0x4C);

	MovePlayerPacket() = default;
};
