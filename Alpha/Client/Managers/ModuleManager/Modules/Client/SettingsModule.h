#pragma once
#include "../Module.h"
#include "../Settings/TextSetting.h"
#include "../Settings/EnumSetting.h"

class SettingsModule : public Module {
public:
	std::string iconText = "+/-";
	std::string prefixText = ".";
	int language = 0;
	bool showMessagePrefix = true;
	std::string messagePrefix;
	std::string messageTag;
	int messagePrefixColor = 11;
	int messageTagColor = 14;
	int messageBracketColor = 8;
	int messageTextColor = 7;
	bool commandSuggestions = true;

	std::string lastSyncedPrefix;

	SettingsModule();

	bool runOnBackground() override { return true; }
	void setEnabled(bool enable) override { Module::setEnabled(true); }
	void toggle() override {}

	void onNormalTick(LocalPlayer* localPlayer) override;

	static std::string getIconText();
	static int getLanguage();
	static std::string formatChatMessage(const std::string& message);
	static bool suggestionsEnabled();
};
