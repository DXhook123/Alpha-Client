#pragma once
#include "../../Utils/MemoryUtil.h"
#include "PlayerListEntry.h"
#include "../GameVersion.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

class Actor;
class PacketSender;
class HitResult;

class Level {
public:
    std::vector<Actor*> getRuntimeActorList() {
        std::vector<Actor*> listOut;
        MemoryUtil::callVirtualFunc<decltype(&listOut)>(325, this, &listOut);
        return listOut;
    }
	PlayerListMap* getPlayerList() {
		if (SdkVersion::is12651())
			return MemoryUtil::callVirtualFunc<PlayerListMap*>(319, this);
		return *reinterpret_cast<PlayerListMap**>(reinterpret_cast<uintptr_t>(this) + 0x4E0);
	}
	HitResult* getHitResult() {
		return MemoryUtil::callVirtualFunc<HitResult*>(335, this);
	}
};
