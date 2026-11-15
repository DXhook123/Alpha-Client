#pragma once
#include "../Utils/MemoryUtil.h"
#include "../Utils/Spdlog.h"
#include <Windows.h>
#include "Core/ClientInstance.h"
#include "Classes/GuiData.h"
#include "Classes/PacketSender.h"
#include "Classes/LocalPlayer.h"
#include "Classes/GameMode.h"
#include "Classes/Actor.h"
#include "Classes/Mob.h"
#include "Classes/Player.h"
#include "Classes/Block.h"
#include "Classes/ItemStack.h"
#include "Classes/ItemStackBase.h"
#include "Classes/PlayerInventory.h"
#include "Classes/Level.h"
#include "../Utils/ImGuiInput.h"
#include <atomic>

class Level;
class Packet;

namespace Game {
	extern ClientInstance* clientInstance;
	extern int viewPerspectiveMode;
	inline std::atomic_bool textInputActive{ false };

	inline void setTextInputActive(bool active) {
		textInputActive.store(active, std::memory_order_relaxed);
	}

	inline bool isTextInputActive() {
		return textInputActive.load(std::memory_order_relaxed);
	}

	void logSDKOffsets();

	inline ClientInstance* getClientInstance() { return clientInstance; }
	inline void setClientInstance(ClientInstance* ci) { clientInstance = ci; }

	inline void restoreMouseAfterOverlay() {
		if (!clientInstance) return;
		if (!isTextInputActive() && clientInstance->getLocalPlayer()
			&& clientInstance->getScreenName() == "hud_screen") {
			clientInstance->grabMouse();
			if (HWND hwnd = ImGuiInput::getGameWindow()) {
				const HWND foreground = GetForegroundWindow();
				if (foreground && GetAncestor(hwnd, GA_ROOT) == foreground) {
					RECT client{};
					if (GetClientRect(hwnd, &client)) {
						POINT tl{client.left, client.top};
						POINT br{client.right, client.bottom};
						ClientToScreen(hwnd, &tl);
						ClientToScreen(hwnd, &br);
						const RECT clip{tl.x, tl.y, br.x, br.y};
						ClipCursor(&clip);
					}
				}
			}
		}
		else
			clientInstance->releaseMouse();
	}

	inline MinecraftGame* getMinecraftGame() {
		if (!clientInstance) return nullptr;
		return clientInstance->mcGame;
	}

	inline void requestLeaveGame() {
		ClientInstance* ci = getClientInstance();
		if (!ci) return;
		static int slot = -2;
		if (slot == -2) {
			slot = -1;
			__try {
				auto* table = *reinterpret_cast<std::uintptr_t**>(ci);
				const uintptr_t game = MemoryUtil::getGameAddress();
				if (game) {
					const uintptr_t target = game + 0x5DA3D10;
					for (int i = 0; i < 700; ++i) {
						if (table[i] == target) { slot = i; break; }
					}
				}
			} __except (EXCEPTION_EXECUTE_HANDLER) { slot = -1; }
		}
		if (slot < 0) return;
		__try {
			auto* table = *reinterpret_cast<std::uintptr_t**>(ci);
			using fn_t = void(__fastcall*)(ClientInstance*, bool, bool);
			reinterpret_cast<fn_t>(table[slot])(ci, true, false);
		} __except (EXCEPTION_EXECUTE_HANDLER) {}
	}

	inline LocalPlayer* getLocalPlayer() {
		if (!clientInstance) return nullptr;
		return clientInstance->getLocalPlayer();
	}
	inline std::string getXuid() {
		if (!SdkVersion::is12651()) {
			auto* game = getMinecraftGame();
			return game ? game->xuid : std::string{};
		}
		auto* player = getLocalPlayer();
		if (!player) return {};
		std::string result;
		using func_t = void(__fastcall*)(Player*, std::string*);
		auto table = *reinterpret_cast<void***>(player);
		reinterpret_cast<func_t>(table[SdkVersion::current().getPlayerXuid])(player, &result);
		return result;
	}

	inline Level* getLevel() {
		if (!clientInstance) return nullptr;
		return clientInstance->getLevel();
	}

	inline GuiData* getGuiData() {
		if (!clientInstance) return nullptr;
		return clientInstance->getGuiData();
	}

	inline PacketSender* getPacketSender() {
		if (!clientInstance) return nullptr;
		return clientInstance->getPacketSender();
	}

	inline void sendPacket(Packet* packet) {
		PacketSender* sender = getPacketSender();
		if (sender && packet)
			sender->send(packet);
	}

	inline void sendPacketToServer(Packet* packet) {
		PacketSender* sender = getPacketSender();
		if (sender && packet)
			sender->sendToServer(packet);
	}

	inline bool canUseMoveKeys() {
		if (!clientInstance || !clientInstance->mcGame) return false;
		return !isTextInputActive();
	}
	inline bool isKeyDown(int vk) {
		return (GetAsyncKeyState(vk) & 0x8000) != 0;
	}
}
