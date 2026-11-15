#pragma once

#include <vector>
#include <imgui.h>
#include "Modules/Module.h"
#include "sdk/Game.h"

class Packet;

class ModuleManager {
public:
	static inline std::vector<Module*> moduleList;

	static void init();
	static void shutdown();
	static void onKeyUpdate(int key, bool isDown);
	static void onImGuiRender(ImDrawList* drawlist, bool hideHud = false);
	static void onNormalTick(LocalPlayer* localPlayer);
	static void onUpdateRotation(LocalPlayer* localPlayer);
	static void onSendPacket(Packet* packet, bool& cancel);
	static void onPacketSent(Packet* packet);
	static void onReceivePacket(Packet* packet, bool& cancel);
	static void onReceivePacketPost(Packet* packet);
	static void onLevelRender();

	template <typename TRet>
	static TRet* getModule() {
		for (Module* mod : moduleList) {
			TRet* result = dynamic_cast<TRet*>(mod);
			if (result)
				return result;
		}
		return nullptr;
	}

	static std::vector<Module*>& getModules() { return moduleList; }
};
