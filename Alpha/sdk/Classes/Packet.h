#pragma once
#include "../../Utils/MemoryUtil.h"
#include <string>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include <string_view>
#include "../GameVersion.h"

enum class PacketID : int32_t {
	Login = 1,
	Text = 9,
	AddPlayer = 12,
	AddActor = 13,
	RemoveActor = 14,
	AddItemActor = 15,
	BossEvent = 74,
	RemoveObjective = 106,
	SetDisplayObjective = 107,
	SetScore = 108,
	SetScoreboardIdentity = 112,
	ActorEvent = 27,
	LevelEvent = 25,
	BlockEvent = 26,
	MovePlayerPacket = 19,
	CorrectPlayerMovePrediction = 161,
	InventoryTransaction = 30,
	MobEquipment = 31,
	ContainerOpen = 46,
	ContainerClose = 47,
	InventoryContent = 49,
	InventorySlot = 50,
	SetActorMotion = 40,
	PlayerAction = 36,
	FullChunkData = 58,
	SubChunkPacket = 174,
	CommandRequest = 77,
	PlaySound = 86,
	SetTitle = 88,
	ToastRequest = 186,
	Emote = 138,
	PlayerSkin = 0x5D,
	ModalFormResponse = 101,
	PlayerAuthInput = 144,
	ItemStackRequest = 147,
};

enum class SerializationMode : int {
	ManualOnly                         = 0,
	SideBySideLogOnMismatch            = 1,
	SideBySideAssertOnMismatch         = 2,
	SemanticSideBySideLogOnMismatch    = 3,
	SemanticSideBySideAssertOnMismatch = 4,
	CerealOnly                         = 5,
};

class Packet {
private:
	char pad_0x0[0x30];
public:
	PacketID getId() {
		return MemoryUtil::callVirtualFunc<PacketID>(1, this);
	}
	std::string getName() {
		if (SdkVersion::is12651()) {
			std::string_view result;
			using func_t = void(__fastcall*)(Packet*, std::string_view*);
			auto table = *reinterpret_cast<void***>(this);
			reinterpret_cast<func_t>(table[2])(this, &result);
			return std::string(result);
		}
		return *MemoryUtil::callVirtualFunc<std::string*>(2, this, std::string());
	}
};

enum class TextPacketType : unsigned char {
	Raw = 0x0,
	Chat = 0x1,
	Translate = 0x2,
	Popup = 0x3,
	JukeboxPopup = 0x4,
	Tip = 0x5,
	SystemMessage = 0x6,
	Whisper = 0x7,
	Announcement = 0x8,
	TextObjectWhisper = 0x9,
	TextObject = 0xA,
	TextObjectAnnouncement = 0xB,
};

class TextPacket : public Packet {
public:
	bool mTranslationNeeded = false;
	char pad_31[0x7];
	std::string mXboxUserId;
	std::string mPlatformChatId;
	std::optional<std::string> mFilteredMessage;
	TextPacketType mType = TextPacketType::Raw;
	char pad_A1[0x7];
	std::string mString;
	std::variant<std::monostate, std::string, std::vector<std::string>> mData;
	int mSerializationMode = static_cast<int>(SerializationMode::CerealOnly);
	char pad_F4[0xC];

	uint8_t getBodyVariant() const {
		return static_cast<uint8_t>(mData.index());
	}

	std::string* getMessage() {
		switch (getBodyVariant()) {
		case 0:
		case 2:
			return &mString;
		case 1:
			return &std::get<std::string>(mData);
		default:
			return nullptr;
		}
	}

	std::string* getAuthor() {
		return getBodyVariant() == 1 ? &mString : nullptr;
	}

	void chat(const std::string& message, const std::string& author,
		const std::string& xuid) {
		mData = message;
		mString = author;
		mXboxUserId = xuid;
		mType = TextPacketType::Chat;
	}
};

static_assert(sizeof(TextPacket) == 0x100, "TextPacket layout mismatch");

class CommandRequestPacket : public Packet {
public:
	std::string command;
};
