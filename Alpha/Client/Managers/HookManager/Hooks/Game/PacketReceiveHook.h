#pragma once
#include "../FuncHook.h"
#include <memory>
#include "../../../../../sdk/Classes/Packet.h"
#include "../../../../../sdk/Signatures.h"
#include "../../../../../sdk/Game.h"
#include "../../../ModuleManager/ModuleManager.h"
#include "../../../../../Utils/Spdlog.h"
#include <MinHook.h>

class PacketReceiveHook {
public:
	static void cacheCreatePacketAddress(uintptr_t address) { sCreatePacketAddress = address; }

private:
	using func_t = void* (__thiscall*)(void*, void*, void*, std::shared_ptr<Packet> const&);
	static inline void* sNetworkIdentifier = nullptr;
	static inline void* sNetEventCallback = nullptr;
	static inline Level* sContextLevel = nullptr;
	static inline thread_local bool sSuppressModuleDispatch = false;

	template <PacketID HookedPacket>
	class PacketHandlerHook : public FuncHook {
		static inline func_t oFunc = nullptr;

		static void* __fastcall hookedHandle(void* _this, void* networkIdentifier,
			void* netEventCallback, std::shared_ptr<Packet> const& packet) {
		FuncHook::CallbackGuard lifetime;
			bool cancel = false;
			if (packet) {
				if (!sSuppressModuleDispatch && networkIdentifier && netEventCallback) {
					sNetworkIdentifier = networkIdentifier;
					sNetEventCallback = netEventCallback;
					sContextLevel = Game::getLevel();
				}
				if (!sSuppressModuleDispatch) {
					ModuleManager::onReceivePacket(packet.get(), cancel);
					if (cancel) return nullptr;
				}
			}
			void* result = oFunc(_this, networkIdentifier, netEventCallback, packet);
			if (packet && !sSuppressModuleDispatch)
				ModuleManager::onReceivePacketPost(packet.get());
			return result;
		}

	public:
		PacketHandlerHook() {
			OriginFunc = (void*)&oFunc;
			func = (void*)&hookedHandle;
		}
	};

	static inline FuncHook* sMotionHook = nullptr;
	static inline FuncHook* sMovePlayerHook = nullptr;
	static inline FuncHook* sTextHook = nullptr;
	static inline uintptr_t sCreatePacketAddress = 0;

	static std::shared_ptr<Packet> createPacketById(PacketID id) {
		using create_t = void (__fastcall*)(std::shared_ptr<Packet>*, int);
		static create_t create = nullptr;
		if (create == nullptr) {
			create = reinterpret_cast<create_t>(sCreatePacketAddress
				? sCreatePacketAddress
				: findSig(Sigs::packet::minecraftPacketsCreatePacket));
		}
		if (!create) return {};
		std::shared_ptr<Packet> packet;
		create(&packet, static_cast<int>(id));
		return packet;
	}
	static uintptr_t getHandlerAddress(Packet* packet) {
		if (!packet) return 0;
		void* dispatcher = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(packet) + 0x20);
		if (!dispatcher) return 0;
		uintptr_t** vt = *reinterpret_cast<uintptr_t***>(dispatcher);
		if (!vt || !vt[1]) return 0;
		return reinterpret_cast<uintptr_t>(vt[1]);
	}

	template <PacketID Id>
	static FuncHook* hookPacketHandler(const char* hookName) {
		std::shared_ptr<Packet> packet = createPacketById(Id);
		if (!packet) {
			LOG_ERROR("[PacketReceiveHook] createPacket(%d) failed", static_cast<int>(Id));
			return nullptr;
		}

		uintptr_t handlerAddr = getHandlerAddress(packet.get());
		if (!handlerAddr) {
			LOG_ERROR("[PacketReceiveHook] packet %d handler address not found", static_cast<int>(Id));
			return nullptr;
		}

		auto* hook = new PacketHandlerHook<Id>();
		hook->name = hookName;
		hook->address = handlerAddr;
		hook->onHookRequest();
		if (hook->enableHook()) {
			LOG_INFO("[PacketReceiveHook] hooked packet %d @ 0x%p", static_cast<int>(Id), (void*)handlerAddr);
			return hook;
		}

		LOG_ERROR("[PacketReceiveHook] ENABLE FAILED: packet %d @ 0x%p", static_cast<int>(Id), (void*)handlerAddr);
		delete hook;
		return nullptr;
	}

public:
	static void clearInjectionContext() {
		sNetworkIdentifier = nullptr;
		sNetEventCallback = nullptr;
		sContextLevel = nullptr;
	}

	static std::shared_ptr<Packet> createPacket(PacketID id) {
		return createPacketById(id);
	}

	static bool canInject() {
		return sNetworkIdentifier != nullptr && sNetEventCallback != nullptr
			&& sContextLevel != nullptr && Game::getLevel() == sContextLevel;
	}

	static bool inject(const std::shared_ptr<Packet>& packet) {
		if (!packet || !canInject()) return false;

		void* dispatcher = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(packet.get()) + 0x20);
		if (!dispatcher) return false;
		uintptr_t handlerAddr = getHandlerAddress(packet.get());
		if (!handlerAddr) return false;

		auto handler = reinterpret_cast<func_t>(handlerAddr);
		handler(dispatcher, sNetworkIdentifier, sNetEventCallback, packet);
		return true;
	}

	static bool injectSilently(const std::shared_ptr<Packet>& packet) {
		if (!packet || !canInject()) return false;

		void* dispatcher = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(packet.get()) + 0x20);
		if (!dispatcher) return false;
		uintptr_t handlerAddr = getHandlerAddress(packet.get());
		if (!handlerAddr) return false;

		auto handler = reinterpret_cast<func_t>(handlerAddr);
		const bool previous = sSuppressModuleDispatch;
		sSuppressModuleDispatch = true;
		handler(dispatcher, sNetworkIdentifier, sNetEventCallback, packet);
		sSuppressModuleDispatch = previous;
		return true;
	}

	static bool init() {
		static bool called = false;
		if (called) return true;
		called = true;

		sMotionHook = hookPacketHandler<PacketID::SetActorMotion>("PacketReceiveHook_SetActorMotion");
		sMovePlayerHook = hookPacketHandler<PacketID::MovePlayerPacket>("PacketReceiveHook_MovePlayer");
		sTextHook = hookPacketHandler<PacketID::Text>("PacketReceiveHook_Text");
		if (MH_ApplyQueued() != MH_OK) {
			LOG_ERROR("[PacketReceiveHook] failed to apply queued hooks");
			return false;
		}

		return sMotionHook != nullptr && sMovePlayerHook != nullptr;
	}

	static void shutdown() {
		delete sTextHook;
		sTextHook = nullptr;
		delete sMovePlayerHook;
		sMovePlayerHook = nullptr;
		delete sMotionHook;
		sMotionHook = nullptr;
		sNetworkIdentifier = nullptr;
		sNetEventCallback = nullptr;
		sContextLevel = nullptr;
	}
};
