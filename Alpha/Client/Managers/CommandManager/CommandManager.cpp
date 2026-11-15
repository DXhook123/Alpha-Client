#include "CommandManager.h"
#include "../../Client.h"
#include "../ModuleManager/ModuleManager.h"
#include "../ModuleManager/Modules/Module.h"
#include "../ModuleManager/Modules/Client/ClickGui.h"
#include "../ModuleManager/Modules/Client/HudEditor.h"
#include "../ModuleManager/Modules/Client/SettingsModule.h"
#include "../ModuleManager/Modules/Settings/BoolSetting.h"
#include "../ModuleManager/Modules/Settings/SliderSetting.h"
#include "../ModuleManager/Modules/Settings/EnumSetting.h"
#include "../ModuleManager/Modules/Settings/TextSetting.h"
#include "../ModuleManager/Modules/Settings/KeybindSetting.h"
#include "../ModuleManager/Modules/Settings/ColorSetting.h"
#include "../../../Utils/MemoryUtil.h"
#include "../../../Utils/Minecraft/InventoryUtil.h"
#include "../../../Utils/FileUtil.h"
#include "../../../Utils/Logger.h"
#include <sstream>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <utility>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <Windows.h>

static ClickGui::WindowPositions parseClickGuiWindowPositions(const json& object) {
	ClickGui::WindowPositions positions;
	if (!object.is_object())
		return positions;
	for (const auto& [name, value] : object.items()) {
		if (!value.is_object() || !value.contains("x") || !value.contains("y") ||
			!value["x"].is_number() || !value["y"].is_number())
			continue;
		positions[name] = Vec2<float>(value["x"].get<float>(), value["y"].get<float>());
	}
	return positions;
}

static ClickGui::WindowExpandedStates parseClickGuiWindowStates(const json& object) {
	ClickGui::WindowExpandedStates states;
	if (!object.is_object())
		return states;
	for (const auto& [name, value] : object.items()) {
		if (value.is_boolean())
			states[name] = value.get<bool>();
	}
	return states;
}

std::string CommandManager::getSettingsPath() {
	return FileUtil::getClientPath() + "Settings\\";
}

CommandBase::CommandBase(std::string _name, std::string _description, std::string _usage, std::vector<std::string> _aliases) {
	this->name = _name;
	this->description = _description;
	this->usage = _usage;
	this->aliases = _aliases;
}

BindCommand::BindCommand() : CommandBase("bind", "Bind a key to a module", "<module> <key>", { "b" }) {}

bool BindCommand::execute(const std::vector<std::string>& args) {
	if (args.size() < 3)
		return false;

	std::string moduleNeedToFind = args[1];
	std::transform(moduleNeedToFind.begin(), moduleNeedToFind.end(), moduleNeedToFind.begin(), ::tolower);
	Module* currentModule = nullptr;

	for (auto& mod : ModuleManager::moduleList) {
		std::string moduleName = mod->getModuleName();
		std::transform(moduleName.begin(), moduleName.end(), moduleName.begin(), ::tolower);
		if (moduleName == moduleNeedToFind) {
			currentModule = mod;
			break;
		}
	}

	if (currentModule == nullptr) {
		Client::displayMessageF("Couldn't find module: %s", moduleNeedToFind.c_str());
		return true;
	}

	std::string key = args[2];
	std::transform(key.begin(), key.end(), key.begin(), ::tolower);

	if (key == "none") {
		currentModule->setKeybind(0x0);
		Client::displayMessageF("Set bind %s -> None", currentModule->getModuleName().c_str());
		CommandManager::markForSave();
		return true;
	}

	if (key.size() > 1) {
		const char* needle = key.c_str();
		for (int i = 0; i < 190; i++) {
			const char* haystack = KeyNames[i];
			if (_stricmp(needle, haystack) == 0) {
				currentModule->setKeybind(i);
				Client::displayMessageF("Set bind %s -> %s", currentModule->getModuleName().c_str(), haystack);
				CommandManager::markForSave();
				return true;
			}
		}
		Client::displayMessageF("Invalid key!");
		return true;
	}

	int keyCode = (int)key[0];
	keyCode = ::toupper(keyCode);
	if (keyCode > 0 && keyCode < 192) {
		currentModule->setKeybind(keyCode);
		Client::displayMessageF("Set bind %s -> %c", currentModule->getModuleName().c_str(), (char)keyCode);
		CommandManager::markForSave();
		return true;
	}

	Client::displayMessageF("Invalid key!");
	return true;
}

ItemIdCommand::ItemIdCommand() : CommandBase("itemid", "Show the id of the held item", "", { "id" }) {}

bool ItemIdCommand::execute(const std::vector<std::string>& args) {
	ItemStack* stack = InventoryUtil::getItem(InventoryUtil::getSelectedSlot());
	if (!InventoryUtil::isValid(stack)) {
		Client::displayMessageF("You are not holding an item!");
		return true;
	}
	Item* item = stack->mItem.get();
	Client::displayMessageF("Item id: %d (%s)", InventoryUtil::getItemId(stack),
		item ? item->mRawNameId.mStr.c_str() : "unknown");
	return true;
}

SaveCommand::SaveCommand() : CommandBase("save", "Save the current config, or save it under a new name", "[name]", { "s" }) {}

bool SaveCommand::execute(const std::vector<std::string>& args) {
	if (args.size() >= 2)
		CommandManager::currentConfig = args[1];
	CommandManager::saveConfig();
	Client::displayMessageF("Saved config %s", CommandManager::currentConfig.c_str());
	return true;
}

EjectCommand::EjectCommand() : CommandBase("eject", "Unload the cheat from the game", "", { "uninject" }) {}

bool EjectCommand::execute(const std::vector<std::string>& args) {
	Client::requestShutdown();
	return true;
}

void CommandManager::saveModuleConfig(Module* mod, json& out) {
	json& modObj = out[mod->getModuleName()];
	modObj["enabled"] = mod->isEnabled();
	modObj["keybind"] = mod->getKeybind();

	json settingsObj = json::object();
	for (Setting* s : mod->getSettingList()) {
		switch (s->type) {
		case SettingType::BOOL_S: {
			BoolSetting* bs = static_cast<BoolSetting*>(s);
			settingsObj[s->name] = *bs->value;
			break;
		}
		case SettingType::SLIDER_S: {
			SliderSettingBase* sb = static_cast<SliderSettingBase*>(s);
			if (sb->valueType == ValueType::INT_T) {
				SliderSetting<int>* is = static_cast<SliderSetting<int>*>(sb);
				settingsObj[s->name] = *is->valuePtr;
			}
			else {
				SliderSetting<float>* fs = static_cast<SliderSetting<float>*>(sb);
				settingsObj[s->name] = *fs->valuePtr;
			}
			break;
		}
		case SettingType::ENUM_S: {
			EnumSetting* es = static_cast<EnumSetting*>(s);
			settingsObj[s->name] = *es->value;
			break;
		}
		case SettingType::TEXT_S: {
			TextSetting* ts = static_cast<TextSetting*>(s);
			settingsObj[s->name] = *ts->value;
			break;
		}
		case SettingType::KEYBIND_S: {
			KeybindSetting* ks = static_cast<KeybindSetting*>(s);
			settingsObj[s->name] = *ks->value;
			break;
		}
		case SettingType::COLOR_S: {
			ColorSetting* cs = static_cast<ColorSetting*>(s);
			settingsObj[s->name] = { { "r", cs->value->r }, { "g", cs->value->g },
				{ "b", cs->value->b }, { "a", cs->value->a } };
			break;
		}
		default:
			break;
		}
	}

	if (mod->getModuleName() == "ClickGui") {
		ClickGui* clickGui = static_cast<ClickGui*>(mod);
		json positionObj = json::object();
		for (const auto& [name, position] : clickGui->getWindowPositions())
			positionObj[name] = { { "x", position.x }, { "y", position.y } };
		if (!positionObj.empty())
			settingsObj["Window Positions"] = std::move(positionObj);
		json stateObj = json::object();
		for (const auto& [name, expanded] : clickGui->getWindowExpandedStates())
			stateObj[name] = expanded;
		if (!stateObj.empty())
			settingsObj["Window States"] = std::move(stateObj);
	}
	modObj["settings"] = settingsObj;
	if (mod->getModuleName() == "HudEditor") {
		HudEditor* editor = static_cast<HudEditor*>(mod);
		modObj["enabled"] = false;
		modObj["Panel Position"] = { { "x", editor->categoryPosition.x }, { "y", editor->categoryPosition.y } };
		json elements = json::object();
		for (HudModule* element : editor->getElements()) {
			saveModuleConfig(element, elements);
			elements[element->getModuleName()]["Layout"] = element->serialize();
		}
		modObj["Elements"] = std::move(elements);
	}
}

void CommandManager::loadModuleConfig(Module* mod, const json& obj) {
	if (!obj.is_object())
		return;

	auto modIt = obj.find(mod->getModuleName());
	if (modIt == obj.end() && mod->getModuleName() == "AntiCheat")
		modIt = obj.find("SingleplayerAntiCheat");
	if (modIt == obj.end() && mod->getModuleName() == "Logger")
		modIt = obj.find("PacketLogger");
	if (modIt == obj.end() || !modIt->is_object())
		return;
	const json& modObj = *modIt;

	if (modObj.contains("keybind") && modObj["keybind"].is_number_integer())
		mod->setKeybind(modObj["keybind"].get<int>());

	bool shouldEnable = mod->getCategory() == Category::Hud ? mod->isEnabled() : false;
	if (modObj.contains("enabled") && modObj["enabled"].is_boolean())
		shouldEnable = modObj["enabled"].get<bool>();

	if (modObj.contains("settings") && modObj["settings"].is_object()) {
		const auto& settingsObj = modObj["settings"];
		if (!settingsObj.contains("Drawn") && settingsObj.contains("ModuleHide")
			&& settingsObj["ModuleHide"].is_boolean())
			mod->setDrawn(!settingsObj["ModuleHide"].get<bool>());
		for (Setting* s : mod->getSettingList()) {
			if (!settingsObj.contains(s->name))
				continue;
			const auto& v = settingsObj[s->name];
			switch (s->type) {
			case SettingType::BOOL_S: {
				BoolSetting* bs = static_cast<BoolSetting*>(s);
				if (v.is_boolean()) *bs->value = v.get<bool>();
				break;
			}
			case SettingType::SLIDER_S: {
				SliderSettingBase* sb = static_cast<SliderSettingBase*>(s);
				if (sb->valueType == ValueType::INT_T) {
					SliderSetting<int>* is = static_cast<SliderSetting<int>*>(sb);
					if (v.is_number_integer()) *is->valuePtr = v.get<int>();
				}
				else {
					SliderSetting<float>* fs = static_cast<SliderSetting<float>*>(sb);
					if (v.is_number()) *fs->valuePtr = v.get<float>();
				}
				break;
			}
			case SettingType::ENUM_S: {
				EnumSetting* es = static_cast<EnumSetting*>(s);
				if (v.is_number_integer()) {
					int value = v.get<int>();
					*es->value = (value >= 0 && value < (int)es->enumList.size()) ? value : 0;
				}
				break;
			}
			case SettingType::TEXT_S: {
				TextSetting* ts = static_cast<TextSetting*>(s);
				if (v.is_string()) *ts->value = v.get<std::string>();
				break;
			}
			case SettingType::KEYBIND_S: {
				KeybindSetting* ks = static_cast<KeybindSetting*>(s);
				if (v.is_number_integer()) *ks->value = v.get<int>();
				break;
			}
			case SettingType::COLOR_S: {
				ColorSetting* cs = static_cast<ColorSetting*>(s);
				if (v.is_object() && v.contains("r") && v.contains("g") && v.contains("b") && v.contains("a")) {
					cs->value->r = (uint8_t)v["r"].get<int>();
					cs->value->g = (uint8_t)v["g"].get<int>();
					cs->value->b = (uint8_t)v["b"].get<int>();
					cs->value->a = (uint8_t)v["a"].get<int>();
					cs->syncHSVFromColor();
				}
				break;
			}
			default:
				break;
			}
		}

		if (mod->getModuleName() == "ClickGui") {
			ClickGui* clickGui = static_cast<ClickGui*>(mod);
			if (settingsObj.contains("Window Positions"))
				clickGui->setWindowPositions(parseClickGuiWindowPositions(settingsObj["Window Positions"]));
			if (settingsObj.contains("Window States"))
				clickGui->setWindowExpandedStates(parseClickGuiWindowStates(settingsObj["Window States"]));
		}
	}

	if (mod->getModuleName() == "HudEditor") {
		HudEditor* editor = static_cast<HudEditor*>(mod);
		if (modObj.contains("Panel Position") && modObj["Panel Position"].is_object()) {
			const auto& position = modObj["Panel Position"];
			if (position.contains("x") && position["x"].is_number() && position.contains("y") && position["y"].is_number())
				editor->categoryPosition = Vec2<float>(position["x"].get<float>(), position["y"].get<float>());
		}
		if (modObj.contains("Elements") && modObj["Elements"].is_object()) {
			const auto& elements = modObj["Elements"];
			for (HudModule* element : editor->getElements()) {
				const auto entry = elements.find(element->getModuleName());
				if (entry == elements.end() || !entry->is_object()) continue;
				if (entry->contains("Layout") && (*entry)["Layout"].is_string())
					element->deserialize((*entry)["Layout"].get<std::string>());
				loadModuleConfig(element, elements);
			}
		}
		shouldEnable = false;
	}
	if (mod->usesHoldActivation())
		shouldEnable = false;
	mod->setEnabled(shouldEnable);
}

bool CommandManager::doesConfigExist(const std::string& name) {
	std::string path = getSettingsPath() + name + ".txt";
	return std::filesystem::exists(path);
}

void CommandManager::createNewConfig(const std::string& name) {
	if (doesConfigExist(name)) {
		Client::displayMessageF("Config %s already exists!", name.c_str());
		return;
	}

	if (currentConfig != "NULL")
		saveConfig();

	currentConfig = name;
	saveConfig();
	Client::displayMessageF("Created config %s!", name.c_str());
}

void CommandManager::loadConfig(const std::string& name) {
	if (!doesConfigExist(name)) {
		Client::displayMessageF("Config %s doesn't exist!", name.c_str());
		return;
	}

	std::string configPath = getSettingsPath() + name + ".txt";
	std::ifstream confFile(configPath, std::ios::in | std::ios::binary);
	if (!confFile.is_open()) {
		Client::displayMessageF("Failed to open config %s!", name.c_str());
		return;
	}

	std::stringstream buffer;
	buffer << confFile.rdbuf();
	confFile.close();

	std::string content = buffer.str();
	if (content.size() >= 3 &&
		(unsigned char)content[0] == 0xEF &&
		(unsigned char)content[1] == 0xBB &&
		(unsigned char)content[2] == 0xBF) {
		content.erase(0, 3);
	}

	if (content.empty()) {
		currentConfig = name;
		Client::displayMessageF("Config %s was empty, loaded defaults.", name.c_str());
		return;
	}

	json parsed;
	try {
		parsed = json::parse(content, nullptr, true, true);
	}
	catch (const json::exception& e) {
		Client::displayMessageF("Failed to parse config %s: %s", name.c_str(), e.what());
		return;
	}
	catch (...) {
		Client::displayMessageF("Failed to parse config %s (unknown error)", name.c_str());
		return;
	}

	if (!parsed.is_object()) {
		Client::displayMessageF("Config %s root is not a JSON object!", name.c_str());
		return;
	}

	currentConfig = name;

	try {
		if (parsed.contains("Hud") && parsed["Hud"].is_object() &&
			parsed["Hud"].contains("settings") && parsed["Hud"]["settings"].is_object() &&
			(!parsed.contains("HudEditor") || parsed["HudEditor"].is_object())) {
			const json legacy = parsed["Hud"]["settings"];
			auto& editor = parsed["HudEditor"];
			if (editor.is_null()) editor = json::object();
			if (!editor.contains("settings")) editor["settings"] = json::object();
			if (!editor.contains("Elements")) editor["Elements"] = json::object();
			if (editor["Elements"].is_object()) {
				for (const char* name : { "Position", "FPS", "Speed", "Ping", "Paperdoll", "ArrayList", "Watermark", "Welcome" }) {
					auto& element = editor["Elements"][name];
					if (element.is_null()) element = json::object();
					if (!element.is_object()) continue;
					const std::string prefix(name);
					if (!element.contains("enabled") && legacy.contains("Show " + prefix))
						element["enabled"] = legacy["Show " + prefix];
					if (!element.contains("settings")) element["settings"] = json::object();
					if (!element["settings"].is_object()) continue;
					for (const char* setting : { "Color", "Opacity", "Size", "Spacing", "Background", "Right Bar", "Always Show", "Text", "Pulse" }) {
						const std::string oldName = prefix + " " + setting;
						if (legacy.contains(oldName) && !element["settings"].contains(setting))
							element["settings"][setting] = legacy[oldName];
					}
				}
			}
		}
		if (parsed.contains("ClickGui") && parsed["ClickGui"].is_object()) {
			const auto& oldGui = parsed["ClickGui"];
			if (oldGui.contains("settings") && oldGui["settings"].is_object() &&
				oldGui["settings"].contains("Font Size") && oldGui["settings"]["Font Size"].is_number() &&
				(!parsed.contains("CustomFont") || parsed["CustomFont"].is_object())) {
				auto& font = parsed["CustomFont"];
				if (!font.contains("settings") || font["settings"].is_object()) {
					auto& settings = font["settings"];
					if (!settings.contains("Font Size"))
						settings["Font Size"] = oldGui["settings"]["Font Size"];
				}
			}
		}
		for (Module* mod : ModuleManager::moduleList)
			loadModuleConfig(mod, parsed);
	}
	catch (...) {
		Client::displayMessageF("Config loaded but some modules failed to apply!");
	}

	Client::displayMessageF("Loaded config %s!", name.c_str());
}

namespace {
	struct PendingConfigWrite {
		std::string configPath;
		std::string configName;
		std::string serialized;
	};

	enum class ConfigWriteError {
		None,
		Open,
		Write,
		Replace
	};

	std::mutex sSaveWorkerMutex;
	std::condition_variable sSaveWorkerCv;
	std::thread sSaveWorkerThread;
	bool sSaveWorkerStarted = false;
	bool sSaveWorkerStop = false;
	bool sSaveJobQueued = false;
	bool sSaveWorkerBusy = false;
	PendingConfigWrite sSaveJob;

	std::atomic<uint64_t> sSaveGeneration{ 0 };

	std::atomic<bool> sSaveResultReady{ false };
	ConfigWriteError sSaveResultError = ConfigWriteError::None;
	DWORD sSaveResultWin32Error = 0;
	std::string sSaveResultConfigName;

	uint64_t sAutoSaveGeneration = 0;
	bool sAutoSaveInFlight = false;

	void saveWorkerLoop() {
		std::unique_lock<std::mutex> lock(sSaveWorkerMutex);
		for (;;) {
			sSaveWorkerCv.wait(lock, [] { return sSaveJobQueued || sSaveWorkerStop; });
			if (!sSaveJobQueued && sSaveWorkerStop)
				return;
			PendingConfigWrite job = std::move(sSaveJob);
			sSaveJob = PendingConfigWrite{};
			sSaveJobQueued = false;
			sSaveWorkerBusy = true;
			lock.unlock();

			ConfigWriteError error = ConfigWriteError::None;
			DWORD win32Error = 0;
			const std::string tempPath = job.configPath + ".tmp";
			std::ofstream temp(tempPath, std::ios::out | std::ios::binary | std::ios::trunc);
			if (!temp.is_open()) {
				error = ConfigWriteError::Open;
			}
			else {
				temp.write(job.serialized.data(), static_cast<std::streamsize>(job.serialized.size()));
				temp.flush();
				const bool writeOk = temp.good();
				temp.close();
				if (!writeOk)
					error = ConfigWriteError::Write;
				else if (!MoveFileExA(tempPath.c_str(), job.configPath.c_str(),
					MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
					error = ConfigWriteError::Replace;
					win32Error = GetLastError();
				}
			}
			if (error != ConfigWriteError::None) {
				std::error_code removeError;
				std::filesystem::remove(tempPath, removeError);
			}

			lock.lock();
			sSaveWorkerBusy = false;
			sSaveResultError = error;
			sSaveResultWin32Error = win32Error;
			sSaveResultConfigName = std::move(job.configName);
			sSaveResultReady.store(true, std::memory_order_release);
			sSaveWorkerCv.notify_all();
		}
	}

	void queueConfigWrite(PendingConfigWrite&& job) {
		{
			std::lock_guard<std::mutex> lock(sSaveWorkerMutex);
			if (!sSaveWorkerStarted) {
				sSaveWorkerThread = std::thread(saveWorkerLoop);
				sSaveWorkerStarted = true;
			}
			sSaveJob = std::move(job);
			sSaveJobQueued = true;
		}
		sSaveWorkerCv.notify_one();
	}

	void drainPendingConfigWrite() {
		std::unique_lock<std::mutex> lock(sSaveWorkerMutex);
		if (!sSaveWorkerStarted)
			return;
		sSaveWorkerCv.wait(lock, [] { return !sSaveJobQueued && !sSaveWorkerBusy; });
	}
}

bool CommandManager::serializeCurrentConfig(json& out) {
	out = json::object();
	try {
		for (Module* mod : ModuleManager::moduleList)
			saveModuleConfig(mod, out);
	}
	catch (...) {
		LOG_ERROR("[CommandManager] Failed to serialize modules");
		return false;
	}
	return true;
}

bool CommandManager::saveConfig() {
	if (currentConfig == "NULL")
		return false;

	drainPendingConfigWrite();

	const std::string path = getSettingsPath();
	if (!std::filesystem::exists(path))
		std::filesystem::create_directories(path);
	const std::string configPath = path + currentConfig + ".txt";

	json root;
	if (!serializeCurrentConfig(root)) {
		Client::displayMessageF("Failed to serialize config!");
		return false;
	}

	std::string serialized;
	try {
		serialized = root.dump(4);
		serialized.push_back('\n');
	}
	catch (...) {
		Client::displayMessageF("Failed to serialize config!");
		return false;
	}

	const std::string tempPath = configPath + ".tmp";
	std::ofstream temp(tempPath, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!temp.is_open()) {
		Client::displayMessageF("Failed to write config %s!", currentConfig.c_str());
		return false;
	}
	temp.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
	temp.flush();
	const bool writeOk = temp.good();
	temp.close();
	if (!writeOk) {
		std::error_code removeError;
		std::filesystem::remove(tempPath, removeError);
		Client::displayMessageF("Failed to write config %s!", currentConfig.c_str());
		return false;
	}

	if (!MoveFileExA(tempPath.c_str(), configPath.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		const DWORD error = GetLastError();
		std::error_code removeError;
		std::filesystem::remove(tempPath, removeError);
		LOG_ERROR("[CommandManager] Failed to replace config %s (Win32 error %lu)",
			configPath.c_str(), static_cast<unsigned long>(error));
		Client::displayMessageF("Failed to replace config %s!", currentConfig.c_str());
		return false;
	}

	needsSave = false;
	return true;
}

void CommandManager::markForSave() {
	if (currentConfig == "NULL")
		return;
	needsSave = true;
	lastSaveTime = std::chrono::steady_clock::now();
	sSaveGeneration.fetch_add(1, std::memory_order_relaxed);
}

void CommandManager::checkAutoSave() {
	if (sSaveResultReady.load(std::memory_order_acquire)) {
		bool tookResult = false;
		ConfigWriteError error = ConfigWriteError::None;
		DWORD win32Error = 0;
		std::string configName;
		{
			std::lock_guard<std::mutex> lock(sSaveWorkerMutex);
			if (sSaveResultReady.load(std::memory_order_relaxed)) {
				error = sSaveResultError;
				win32Error = sSaveResultWin32Error;
				configName = std::move(sSaveResultConfigName);
				sSaveResultConfigName.clear();
				sSaveResultReady.store(false, std::memory_order_relaxed);
				tookResult = true;
			}
		}
		if (tookResult) {
			sAutoSaveInFlight = false;
			if (error != ConfigWriteError::None) {
				lastSaveTime = std::chrono::steady_clock::now();
				if (error == ConfigWriteError::Replace) {
					LOG_ERROR("[CommandManager] Failed to replace config %s (Win32 error %lu)",
						configName.c_str(), static_cast<unsigned long>(win32Error));
					Client::displayMessageF("Failed to replace config %s!", configName.c_str());
				}
				else {
					Client::displayMessageF("Failed to write config %s!", configName.c_str());
				}
			}
			else if (sSaveGeneration.load(std::memory_order_relaxed) == sAutoSaveGeneration) {
				needsSave = false;
			}
		}
	}

	if (!needsSave || currentConfig == "NULL")
		return;
	if (sAutoSaveInFlight)
		return;

	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSaveTime).count();
	if (elapsed < AUTO_SAVE_DELAY_MS)
		return;

	sAutoSaveGeneration = sSaveGeneration.load(std::memory_order_relaxed);

	const std::string path = getSettingsPath();
	if (!std::filesystem::exists(path))
		std::filesystem::create_directories(path);
	const std::string configPath = path + currentConfig + ".txt";

	json root;
	if (!serializeCurrentConfig(root)) {
		Client::displayMessageF("Failed to serialize config!");
		lastSaveTime = now;
		return;
	}
	std::string serialized;
	try {
		serialized = root.dump(4);
		serialized.push_back('\n');
	}
	catch (...) {
		Client::displayMessageF("Failed to serialize config!");
		lastSaveTime = now;
		return;
	}

	PendingConfigWrite job;
	job.configPath = configPath;
	job.configName = currentConfig;
	job.serialized = std::move(serialized);
	sAutoSaveInFlight = true;
	queueConfigWrite(std::move(job));
}

void CommandManager::init() {
	std::string path = getSettingsPath();
	if (!std::filesystem::exists(path))
		std::filesystem::create_directories(path);
	configsPath = path;

	commandList.push_back(new BindCommand());
	commandList.push_back(new ItemIdCommand());
	commandList.push_back(new SaveCommand());
	commandList.push_back(new EjectCommand());

	LOG_INFO("[CommandManager] Initialized %d commands, prefix='%c'", (int)commandList.size(), prefix);
}

void CommandManager::shutdown() {
	shutdownSaveWorker();
	for (auto& command : commandList) {
		delete command;
		command = nullptr;
	}
	commandList.clear();
	chatInput.clear();
}

void CommandManager::shutdownSaveWorker() {
	std::thread worker;
	{
		std::lock_guard<std::mutex> lock(sSaveWorkerMutex);
		if (!sSaveWorkerStarted)
			return;
		sSaveWorkerStop = true;
		worker = std::move(sSaveWorkerThread);
		sSaveWorkerStarted = false;
	}
	sSaveWorkerCv.notify_all();
	if (worker.joinable())
		worker.join();
}

void CommandManager::resetChatInput() {
	chatInput.clear();
}

void CommandManager::handleChatInputMessage(UINT message, WPARAM wParam) {
	if (message == WM_CHAR) {
		const unsigned int character = static_cast<unsigned int>(wParam);
		if (character == 8) {
			if (!chatInput.empty()) chatInput.pop_back();
		} else if (character >= 0x20 && character < 0x7f && chatInput.size() < 255) {
			chatInput.push_back(static_cast<char>(character));
		}
	}
}

void CommandManager::renderSuggestions(ImDrawList* drawList) {
	if (!drawList || !SettingsModule::suggestionsEnabled() || !Game::isTextInputActive()
		|| chatInput.empty() || chatInput.front() != prefix || !ImGui::GetCurrentContext()) return;

	std::string query = chatInput.substr(1);
	const size_t separator = query.find_first_of(" \t");
	if (separator != std::string::npos) query.resize(separator);
	std::transform(query.begin(), query.end(), query.begin(), [](unsigned char value) {
		return static_cast<char>(std::tolower(value));
	});

	std::vector<CommandBase*> matches;
	for (CommandBase* command : commandList) {
		if (!command) continue;
		std::string name = command->name;
		std::transform(name.begin(), name.end(), name.begin(), [](unsigned char value) {
			return static_cast<char>(std::tolower(value));
		});
		bool match = query.empty() || name.starts_with(query);
		for (const std::string& alias : command->aliases) {
			std::string lower = alias;
			std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char value) {
				return static_cast<char>(std::tolower(value));
			});
			match = match || lower.starts_with(query);
		}
		if (match) matches.push_back(command);
	}
	if (matches.empty()) return;

	const ImVec2 display = ImGui::GetIO().DisplaySize;
	const float scale = (std::max)(1.f, display.y / 1080.f);
	const float lineHeight = 22.f * scale;
	const float left = 42.f * scale;
	const float bottom = display.y - 64.f * scale;
	float width = 260.f * scale;
	std::vector<std::string> lines;
	for (CommandBase* command : matches) {
		std::string line(1, prefix);
		line += command->name;
		if (!command->usage.empty()) line += " " + command->usage;
		line += "  -  ";
		line += command->description;
		width = (std::max)(width, ImGui::CalcTextSize(line.c_str()).x + 20.f * scale);
		lines.push_back(std::move(line));
	}
	const float top = bottom - lineHeight * static_cast<float>(lines.size());
	drawList->AddRectFilled(ImVec2(left, top), ImVec2(left + width, bottom), IM_COL32(12, 14, 18, 220), 4.f * scale);
	drawList->AddRect(ImVec2(left, top), ImVec2(left + width, bottom), IM_COL32(70, 190, 255, 220), 4.f * scale);
	for (size_t index = 0; index < lines.size(); ++index) {
		drawList->AddText(ImVec2(left + 9.f * scale, top + (index * lineHeight) + 3.f * scale),
			IM_COL32(235, 245, 255, 255), lines[index].c_str());
	}
}

void CommandManager::execute(const std::string& message) {
	if (message.empty() || message[0] != prefix)
		return;

	std::string currentMessage = message;
	currentMessage.erase(currentMessage.begin());

	std::vector<std::string> args;
	std::stringstream ss(currentMessage);
	std::string word;
	while (ss >> word)
		args.push_back(word);

	if (args.empty())
		return;

	std::string cmd = args[0];
	std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

	for (auto& command : commandList) {
		std::vector<std::string> checkList;
		checkList.push_back(command->name);
		for (const std::string& alias : command->aliases)
			checkList.push_back(alias);

		for (std::string& check : checkList) {
			std::transform(check.begin(), check.end(), check.begin(), ::tolower);
			if (cmd == check) {
				if (!command->execute(args)) {
					Client::displayMessageF("Failed to execute command '%s'", message.c_str());
					Client::displayMessageF("Usage: %c%s %s", prefix, command->name.c_str(), command->usage.c_str());
				}
				return;
			}
		}
	}

	Client::displayMessageF("Couldn't find command '%s'!", cmd.c_str());
}
