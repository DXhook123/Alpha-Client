#pragma once
#include "Mob.h"
#include "PacketSender.h"
#include <string>

class PlayerInventory;
class GameMode;
class InventoryTransactionManager;

class Player : public Mob {
public:
	CLASS_MEMBER(int, score, 0x488);
	CLASS_MEMBER(std::string, uniqueName, 0x490);
	CLASS_MEMBER(std::string, serverId, 0x4B0);
	CLASS_MEMBER(uint64_t, clientRandomId, 0x510);
	CLASS_MEMBER(std::string, platformId, 0x518);
	CLASS_MEMBER(uint64_t, interactTarget, 0x550);
	CLASS_MEMBER(Vec3<float>, interactTargetPos, 0x558);
	CLASS_MEMBER(PlayerInventory*, playerInventory, 0x5B8);
	CLASS_MEMBER(Vec3<float>, capePosO, 0x588);
	CLASS_MEMBER(Vec3<float>, capePos, 0x594);
	CLASS_MEMBER(Vec3<float>, respawnOriginalPosition, 0x6D8);
	CLASS_MEMBER(std::string, respawnMessage, 0x6E8);
	CLASS_MEMBER(bool, respawnReady, 0x708);
	CLASS_MEMBER(bool, isInitialSpawnDone, 0x709);
	CLASS_MEMBER(int, previousCarriedItem, 0x7E0);
	CLASS_MEMBER(PacketSender*, packetSender, 0x7F8);
	CLASS_MEMBER(GameMode*, gameMode, 0xAA0);
	CLASS_MEMBER(float, serverBuildRatio, 0xAFC);
	CLASS_MEMBER(unsigned char, clientId, 0xB00);
	CLASS_MEMBER(bool, destroyingBlock, 0xB22);
	CLASS_MEMBER(bool, playerLevelChanged, 0xB23);
	CLASS_MEMBER(int, previousLevelRequirement, 0xB24);
	CLASS_MEMBER(unsigned int, chunkRadius, 0xB94);
	CLASS_MEMBER(int64_t, startedBlockingTimeStamp, 0xBA8);
	CLASS_MEMBER(std::string, name, 0xBC0);
	CLASS_MEMBER(std::string, deviceId, 0xC88);
	CLASS_MEMBER(bool, isHostingPlayer, 0xCAA);

public:
	GameMode* getGameMode() { return gameMode; }
	PacketSender* getPacketSender() { return packetSender; }
	std::string& getName() { return name; }
};
