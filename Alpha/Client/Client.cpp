#include "Client.h"
#include "Utils/Logger.h"
#include "Utils/Spdlog.h"
#include "Utils/FileUtil.h"
#include "Managers/HookManager/HookManager.h"
#include "Managers/ModuleManager/ModuleManager.h"
#include "Managers/ModuleManager/Modules/Client/HudEditor.h"
#include "Managers/ModuleManager/Modules/Client/SettingsModule.h"
#include "Managers/CommandManager/CommandManager.h"
#include "sdk/Game.h"
#include "sdk/Core/ClientInstance.h"
#include <cstdarg>

void Client::displayMessage(const std::string& message) {
	auto* ci = Game::getClientInstance();
	if (!ci) return;
	const auto gui = ci->getGuiDataReference();
	if (gui.pointer) gui.pointer->displayMessage(SettingsModule::formatChatMessage(message), SdkVersion::is12651());
}

void Client::displayMessageF(const char* fmt, ...) {
	if (!fmt) return;
	char buf[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, args);
	va_end(args);
	displayMessage(buf);
}

bool Client::init() {
	shuttingDown.store(false, std::memory_order_release);
	featuresInitialized.store(false, std::memory_order_release);
	initialized.store(true, std::memory_order_release);

	FileUtil::createPath(FileUtil::getClientPath());
	Spdlog::init();
	Logger::init();

	ModuleManager::init();
	CommandManager::init();
	featuresInitialized.store(true, std::memory_order_release);
	if (!CommandManager::doesConfigExist("Alpha")) CommandManager::createNewConfig("Alpha");
	else CommandManager::loadConfig("Alpha");

	if (!HookManager::initRenderHooks() || !HookManager::initGameHooks()) {
		LOG_ERROR("[Client] Hook initialization failed");
		return false;
	}
	LOG_INFO("[Client] Alpha initialized");
	return true;
}

void Client::requestShutdown() {
	initialized.store(false, std::memory_order_release);
}

void Client::shutdown() {
	bool expected = false;
	if (!shuttingDown.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
	if (featuresInitialized.load(std::memory_order_acquire)) {
		HudEditor::saveHudConfig();
		if (CommandManager::currentConfig != "NULL") CommandManager::saveConfig();
	}
	for (auto* mod : ModuleManager::moduleList)
		if (mod->isEnabled()) mod->setEnabled(false);
	initialized.store(false, std::memory_order_release);
}
