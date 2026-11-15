#include "PingHUD.h"
#include "Client/Managers/HookManager/Hooks/Game/PingHook.h"
#include "sdk/Game.h"
#include <cstdio>

void PingHUD::onImGuiRender(ImDrawList* drawlist) {
	static Level* lastLevel = nullptr;
	Level* level = Game::getLevel();
	if (level != lastLevel) {
		PingHook::reset();
		lastLevel = level;
	}

	if (!Game::getLocalPlayer()) return;

	char pingBuf[24];
	snprintf(pingBuf, sizeof(pingBuf), "%d ms", PingHook::getAveragePing());
	drawLabeledRect("Ping", pingBuf);
}
