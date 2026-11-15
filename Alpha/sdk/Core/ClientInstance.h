#pragma once
#include "../../Utils/MemoryUtil.h"
#include "../../Utils/Spdlog.h"
#include "../Signatures.h"
#include "MinecraftGame.h"
#include "MinecraftSim.h"
#include "../Classes/GuiData.h"
#include "../Classes/PacketSender.h"
#include "../Classes/BlockSource.h"
#include "../GameVersion.h"
#include <memory>

class LocalPlayer;
class Level;
class LoopbackPacketSender;
class LevelRenderer;

class ClientInstance {
public:
	struct GuiDataReference {
		std::shared_ptr<void> controlBlock;
		GuiData* pointer = nullptr;
	};
	static_assert(sizeof(GuiDataReference) == 24);

	CLASS_MEMBER(MinecraftGame*, mcGame, SdkVersion::current().mcGame);
	CLASS_MEMBER(Minecraft*, minecraft, SdkVersion::current().minecraft);
	CLASS_MEMBER(LevelRenderer*, levelRenderer, SdkVersion::current().levelRenderer);

public:
	LocalPlayer* getLocalPlayer() {
		return MemoryUtil::callVirtualFunc<LocalPlayer*>(SdkVersion::current().getLocalPlayer, this);
	}
	BlockSource* getRegion() {
		return MemoryUtil::callVirtualFunc<BlockSource*>(SdkVersion::current().getRegion, this);
	}

	Level* getLevel() {
		return MemoryUtil::callVirtualFunc<Level*>(SdkVersion::current().getLevel, this);
	}

	GuiDataReference getGuiDataReference() {
		if (SdkVersion::is12651()) {
			using func_t = void(__fastcall*)(ClientInstance*, GuiDataReference*);
			auto table = *reinterpret_cast<uintptr_t**>(this);
			const auto index = SdkVersion::current().getGuiData;
			if (!table || !table[index]) return {};
			GuiDataReference result;
			reinterpret_cast<func_t>(table[index])(this, &result);
			return result;
		}
		return { {}, *reinterpret_cast<GuiData**>(
			reinterpret_cast<uintptr_t>(this) + SdkVersion::current().guiData) };
	}

	GuiData* getGuiData() {
		return getGuiDataReference().pointer;
	}

	LoopbackPacketSender* getLoopbackPacketSender() {
		if (SdkVersion::is12651())
			return reinterpret_cast<LoopbackPacketSender*>(getPacketSender());
		return reinterpret_cast<LoopbackPacketSender*>(*reinterpret_cast<uintptr_t*>(
			reinterpret_cast<uintptr_t>(this) + SdkVersion::current().loopbackPacketSender));
	}

	PacketSender* getPacketSender() {
		return MemoryUtil::callVirtualFunc<PacketSender*>(SdkVersion::current().getPacketSender, this);
	}

	bool getMouseGrabbed() {
		return MemoryUtil::callVirtualFunc<bool>(SdkVersion::current().getMouseGrabbed, this);
	}

	std::string getScreenName() {
		using func_t = std::string*(__fastcall*)(ClientInstance*, std::string*);
		static uintptr_t addr = MemoryUtil::findSignature(Sigs::clientInstance::getScreenName);
		std::string screenName;
		if (addr != 0) {
			reinterpret_cast<func_t>(addr)(this, &screenName);
			return screenName;
		}
		return MemoryUtil::callVirtualFunc<std::string&, std::string&>(
			SdkVersion::current().getScreenName, this, screenName);
	}

	void grabMouse() {
		static uintptr_t addr = MemoryUtil::findSignature(Sigs::clientInstance::grabMouse);
		if (addr != 0) {
			MemoryUtil::callFastcall<void>(addr, this);
			return;
		}
		LOG_WARN("[ClientInstance] grabMouse signature NOT found, refocusing mouse only");
		refocusMouse();
	}
	void releaseMouse() {
		static uintptr_t addr = MemoryUtil::findSignature(Sigs::clientInstance::releaseMouse);
		if (addr != 0) {
			MemoryUtil::callFastcall<void>(addr, this);
			return;
		}
		LOG_WARN("[ClientInstance] releaseMouse signature NOT found");
	}

	void refocusMouse() {
		if (SdkVersion::is12651())
			MemoryUtil::callVirtualFunc<void, bool>(SdkVersion::current().refocusMouse, this, false);
		else
			MemoryUtil::callVirtualFunc<void>(SdkVersion::current().refocusMouse, this);
	}

	void playUI() {
		if (!SdkVersion::is12651()) MemoryUtil::callVirtualFunc<void>(321, this);
	}
};
