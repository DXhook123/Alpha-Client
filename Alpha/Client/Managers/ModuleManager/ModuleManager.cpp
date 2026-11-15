#include "ModuleManager.h"

#include <algorithm>
#include <Windows.h>
#include "Utils/Minecraft/WorldUtil.h"
#include "Modules/Client/ClickGui.h"
#include "Modules/Client/CustomFont.h"
#include "Modules/Client/ColorModule.h"
#include "Modules/Client/SettingsModule.h"
#include "Modules/Client/DebugInfo.h"
#include "Modules/Client/TabList.h"
#include "Modules/Client/HudEditor.h"
#include "Modules/Combat/FeetTrap.h"
#include "Modules/Render/Fullbright.h"
#include "../CommandManager/CommandManager.h"
#include "Utils/Spdlog.h"

void ModuleManager::init() {
	moduleList.push_back(new ClickGui());
	moduleList.push_back(new CustomFont());
	moduleList.push_back(new ColorModule());
	moduleList.push_back(new SettingsModule());
	moduleList.push_back(new DebugInfo());
	moduleList.push_back(new TabList());
	moduleList.push_back(new HudEditor());
	moduleList.push_back(new FeetTrap());
	moduleList.push_back(new Fullbright());

	std::sort(moduleList.begin(), moduleList.end(), [](Module* lhs, Module* rhs) {
		if (lhs->getTickPriority() != rhs->getTickPriority())
			return lhs->getTickPriority() > rhs->getTickPriority();
		return lhs->getModuleName() < rhs->getModuleName();
	});

	if (auto* cm = getModule<ColorModule>()) cm->setEnabled(true);
	if (auto* sm = getModule<SettingsModule>()) sm->setEnabled(true);

	HudEditor::loadHudConfig();

	LOG_INFO("[ModuleManager] Initialized %d modules", (int)moduleList.size());
}
void ModuleManager::shutdown() {
	for (Module* mod : moduleList)
		delete mod;
	moduleList.clear();
}

void ModuleManager::onKeyUpdate(int key, bool isDown) {
	ClickGui* clickGui = getModule<ClickGui>();
	HudEditor* hudEditor = getModule<HudEditor>();

	if (!isDown) {
		for (Module* mod : moduleList) {
			if (!mod || !mod->usesHoldActivation() || mod->getKeybind() == 0 || key != mod->getKeybind())
				continue;
			mod->setEnabled(false);
		}
	}

	if (hudEditor && hudEditor->isEnabled()) {
		if (clickGui && (clickGui->capturingTextSettingPtr || key == VK_SHIFT)) {
			clickGui->onKeyUpdate(key, isDown);
			return;
		}
		if (isDown && (key == VK_ESCAPE || key == hudEditor->getKeybind())) {
			hudEditor->setEnabled(false);
			CommandManager::markForSave();
		}
		return;
	}

	if (clickGui && clickGui->isEnabled()) {
		clickGui->onKeyUpdate(key, isDown);
		return;
	}
	if (!Game::canUseMoveKeys()) {
		if (isDown && clickGui && key == clickGui->getKeybind()) {
			clickGui->setEnabled(true);
			CommandManager::markForSave();
		}
		return;
	}

	if (isDown) {
		for (Module* mod : moduleList) {
			if (mod == clickGui) continue;
			const int kb = mod->getKeybind();
			if (kb == 0 || key != kb) continue;

			const bool wasEnabled = mod->isEnabled();
			if (mod->usesHoldActivation())
				mod->setEnabled(true);
			else
				mod->toggle();

			if (wasEnabled != mod->isEnabled()) {
				if (!mod->usesHoldActivation())
					CommandManager::markForSave();
			}
		}
	}

	for (Module* mod : moduleList)
		mod->onKeyUpdate(key, isDown);
}

void ModuleManager::onImGuiRender(ImDrawList* drawlist, bool hideHud) {
	HudEditor* hudEditor = getModule<HudEditor>();
	for (Module* mod : moduleList) {
		if (hideHud && (mod->getCategory() == Category::Hud || dynamic_cast<HudEditor*>(mod))
			&& !(mod == hudEditor && hudEditor->isEnabled()))
			continue;
		if (mod->isEnabled() || mod->runOnBackground())
			mod->onImGuiRender(drawlist);
	}
}

void ModuleManager::onNormalTick(LocalPlayer* localPlayer) {
	WorldUtil::beginSeenPercentTick();
	for (Module* mod : moduleList) {
		if (!mod || !mod->usesHoldActivation() || !mod->isEnabled()) continue;
		const int kb = mod->getKeybind();
		if (kb == 0 || !Game::isKeyDown(kb)) {
			mod->setEnabled(false);
		}
	}
	for (Module* mod : moduleList) {
		if (mod->isEnabled() || mod->runOnBackground())
			mod->onNormalTick(localPlayer);
	}
}
void ModuleManager::onUpdateRotation(LocalPlayer* localPlayer) {

	for (auto& mod : moduleList) {
		if (!mod) continue;
		if (mod->isEnabled() || mod->runOnBackground()) {
			mod->onUpdateRotation(localPlayer);
		}
	}
}
void ModuleManager::onSendPacket(Packet* packet, bool& cancel) {
	for (Module* mod : moduleList) {
		if (!mod) continue;
		if (mod->isEnabled() || mod->runOnBackground())
			mod->onSendPacket(packet, cancel);
	}
}
void ModuleManager::onPacketSent(Packet* packet) {
	for (Module* mod : moduleList) {
		if (!mod) continue;
		if (mod->isEnabled() || mod->runOnBackground())
			mod->onPacketSent(packet);
	}
}
void ModuleManager::onReceivePacket(Packet* packet, bool& cancel) {
	for (Module* mod : moduleList) {
		if (!mod) continue;
		if (mod->isEnabled() || mod->runOnBackground())
			mod->onReceivePacket(packet, cancel);
	}
}

void ModuleManager::onReceivePacketPost(Packet* packet) {
	for (Module* mod : moduleList) {
		if (!mod) continue;
		if (mod->isEnabled() || mod->runOnBackground())
			mod->onReceivePacketPost(packet);
	}
}

void ModuleManager::onLevelRender() {
	for (Module* mod : moduleList) {
		if (!mod) continue;
		if (mod->isEnabled() || mod->runOnBackground())
			mod->onLevelRender();
	}
}

UIColor ColorModule::getThemeColor() {
	ColorModule* cm = ModuleManager::getModule<ColorModule>();
	if (!cm) return UIColor(255, 255, 255, 255);
	if (cm->colorMode != 0) return cm->getColor();
	switch (cm->presetMode) {
	case 1: return UIColor(245, 169, 184, 255);
	case 2: return UIColor(255, 183, 197, 255);
	case 3: return UIColor(200, 168, 233, 255);
	case 4: return UIColor(255, 110, 199, 255);
	default: return cm->themeColor;
	}
}
