#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <Windows.h>
#include <imgui.h>
#include "../../../Libs/json.hpp"

using json = nlohmann::json;

class CommandBase {
public:
	std::string name;
	std::string description;
	std::string usage;
	std::vector<std::string> aliases;

	CommandBase(std::string _name, std::string _description, std::string _usage, std::vector<std::string> _aliases = {});
	virtual bool execute(const std::vector<std::string>& args) = 0;
	virtual ~CommandBase() = default;
};

class BindCommand : public CommandBase {
public:
	BindCommand();
	bool execute(const std::vector<std::string>& args) override;
};

class ItemIdCommand : public CommandBase {
public:
	ItemIdCommand();
	bool execute(const std::vector<std::string>& args) override;
};

class SaveCommand : public CommandBase {
public:
	SaveCommand();
	bool execute(const std::vector<std::string>& args) override;
};

class EjectCommand : public CommandBase {
public:
	EjectCommand();
	bool execute(const std::vector<std::string>& args) override;
};

class CommandManager {
private:
	static inline bool needsSave = false;
	static inline std::chrono::steady_clock::time_point lastSaveTime;
	static inline const int AUTO_SAVE_DELAY_MS = 1000;

	static void saveModuleConfig(class Module* mod, json& out);
	static void loadModuleConfig(class Module* mod, const json& obj);

public:
	static inline std::vector<CommandBase*> commandList;
	static inline char prefix = '.';
	static inline std::string currentConfig = "NULL";
	static inline std::string configsPath;
	static inline std::string chatInput;
	static std::string getSettingsPath();

	static void init();
	static void shutdown();
	static void execute(const std::string& message);
	static void handleChatInputMessage(UINT message, WPARAM wParam);
	static void resetChatInput();
	static void renderSuggestions(ImDrawList* drawList);

	static bool doesConfigExist(const std::string& name);
	static void createNewConfig(const std::string& name);
	static bool serializeCurrentConfig(json& out);
	static void loadConfig(const std::string& name);
	static bool saveConfig();
	static void markForSave();
	static void checkAutoSave();
	static void shutdownSaveWorker();
};
