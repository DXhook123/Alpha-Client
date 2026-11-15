#include "SettingsModule.h"

#include "../../ModuleManager.h"
#include "../../../CommandManager/CommandManager.h"
#include "Utils/ChatMessage.h"

namespace {
	const ChatMessage::Style kDefaultChatStyle{ true, "Alpha", "!", 11, 14, 8, 7 };
	const std::vector<std::string> kChatColors = {
		"Black", "Dark Blue", "Dark Green", "Dark Aqua", "Dark Red", "Dark Purple",
		"Gold", "Gray", "Dark Gray", "Blue", "Green", "Aqua", "Red", "Light Purple", "Yellow", "White"
	};
}

SettingsModule::SettingsModule()
	: Module("Settings", "Customize interface, chat messages and command suggestions", Category::CLIENT, 0) {
	registerSetting(new TextSetting("Expand Icon", "Fold/Expand icon (use / to split)", &iconText, "+/-", 32));
	registerSetting(new TextSetting("Cmd Prefix", "Single ASCII command trigger character", &prefixText, ".", 1));
	registerSetting(new EnumSetting("Language", "UI language: English / Chinese", { "English", "中文" }, &language, 0));
	registerSetting(new BoolSetting("Message Prefix", "Show the shared prefix on client chat messages", &showMessagePrefix, true));
	registerSetting(new TextSetting("Prefix Text", "Text inside the first pair of brackets", &messagePrefix, kDefaultChatStyle.prefix, 64));
	registerSetting(new TextSetting("Tag Text", "Text inside the second pair of brackets; empty hides it", &messageTag, kDefaultChatStyle.tag, 32));
	registerSetting(new EnumSetting("Prefix Color", "Chat prefix color", kChatColors, &messagePrefixColor, kDefaultChatStyle.prefixColor));
	registerSetting(new EnumSetting("Tag Color", "Chat tag color", kChatColors, &messageTagColor, kDefaultChatStyle.tagColor));
	registerSetting(new EnumSetting("Bracket Color", "Chat bracket color", kChatColors, &messageBracketColor, kDefaultChatStyle.bracketColor));
	registerSetting(new EnumSetting("Message Color", "Default chat message color", kChatColors, &messageTextColor, kDefaultChatStyle.textColor));
	registerSetting(new BoolSetting("Command Suggestions", "Show matching commands while typing in chat", &commandSuggestions, true));
}

void SettingsModule::onNormalTick(LocalPlayer*) {
	if (prefixText != lastSyncedPrefix) {
		lastSyncedPrefix = prefixText;
		const unsigned char value = prefixText.empty() ? 0 : static_cast<unsigned char>(prefixText.front());
		CommandManager::prefix = value > 0x20 && value < 0x7f ? static_cast<char>(value) : '.';
	}
}

std::string SettingsModule::getIconText() {
	auto* settings = ModuleManager::getModule<SettingsModule>();
	return settings ? settings->iconText : "+/-";
}

int SettingsModule::getLanguage() {
	auto* settings = ModuleManager::getModule<SettingsModule>();
	return settings ? settings->language : 0;
}

std::string SettingsModule::formatChatMessage(const std::string& message) {
	auto* settings = ModuleManager::getModule<SettingsModule>();
	if (!settings) return ChatMessage::format(message, kDefaultChatStyle);
	return ChatMessage::format(message, { settings->showMessagePrefix, settings->messagePrefix,
		settings->messageTag, settings->messagePrefixColor, settings->messageTagColor,
		settings->messageBracketColor, settings->messageTextColor });
}

bool SettingsModule::suggestionsEnabled() {
	auto* settings = ModuleManager::getModule<SettingsModule>();
	return !settings || settings->commandSuggestions;
}
