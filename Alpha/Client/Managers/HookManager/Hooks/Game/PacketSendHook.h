#pragma once
#include "../FuncHook.h"
#include "sdk/Classes/LoopbackPacketSender.h"
#include "sdk/Classes/Packet.h"
#include "sdk/Game.h"
#include "Client/Managers/CommandManager/CommandManager.h"
#include "Utils/Spdlog.h"
#include "Client/Managers/ModuleManager/ModuleManager.h"
#include <string>
#include <cstring>

class PacketSendHook : public FuncHook {
private:
	using func_t = void(__fastcall*)(PacketSender* sender, Packet* packet);
	static inline func_t oFunc = nullptr;

	static int safeGetPacketId(Packet* packet) {
		if (!packet) return -1;
		int id = -1;
		__try {
			id = (int)packet->getId();
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			id = -2;
		}
		return id;
	}

	static bool tryReadStringAt(void* base, size_t offset, char* outBuf, size_t outSize) {
		if (!base || !outBuf || outSize == 0) return false;
		outBuf[0] = '\0';

		uintptr_t strAddr = reinterpret_cast<uintptr_t>(base) + offset;
		bool ok = false;
		__try {
			size_t sz = *reinterpret_cast<size_t*>(strAddr + 0x10);
			size_t cap = *reinterpret_cast<size_t*>(strAddr + 0x18);

			if (sz == 0 || sz >= outSize) return false;

			const char* dataPtr;
			if (cap <= 15) {
				dataPtr = reinterpret_cast<const char*>(strAddr);
			} else {
				dataPtr = *reinterpret_cast<const char**>(strAddr);
			}

			if (!dataPtr) return false;
			memcpy(outBuf, dataPtr, sz);
			outBuf[sz] = '\0';
			ok = true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			ok = false;
			outBuf[0] = '\0';
		}
		return ok;
	}

	static bool tryReadTextMessage(TextPacket* tp, char* outBuf, size_t outSize) {
		if (!tp || !outBuf || outSize == 0) return false;
		outBuf[0] = '\0';
		__try {
			std::string* message = tp->getMessage();
			if (!message || message->empty() || message->size() >= outSize)
				return false;
			memcpy(outBuf, message->data(), message->size());
			outBuf[message->size()] = '\0';
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			outBuf[0] = '\0';
			return false;
		}
	}

	static void PacketSender_sendToServer(PacketSender* _this, Packet* packet) {
		FuncHook::CallbackGuard lifetime;
		bool shouldCancel = false;

		if (packet) {
			int id = safeGetPacketId(packet);
			if (id == static_cast<int>(PacketID::Text)) {
				auto* tp = reinterpret_cast<TextPacket*>(packet);
				char buf[256];
				if (tryReadTextMessage(tp, buf, sizeof(buf))) {
					if (buf[0] == CommandManager::prefix) {
						LOG_INFO("[PacketSend] executing command: %s", buf);
						CommandManager::execute(std::string(buf));
						return;
					}
				}
			}
			ModuleManager::onSendPacket(packet, shouldCancel);
		}

		if (shouldCancel) return;
		oFunc(_this, packet);
		ModuleManager::onPacketSent(packet);
	}

public:
	PacketSendHook() {
		OriginFunc = (void*)&oFunc;
		func = (void*)&PacketSender_sendToServer;
	}
};
