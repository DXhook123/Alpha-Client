#pragma once
#include "Setting.h"
#include <string>

class TextSetting : public Setting {
public:
	std::string* value = nullptr;
	std::string defaultValue;
	std::string backupValue;
	std::string editBuffer;
	size_t maxLength = 64;
	bool editing = false;

	TextSetting(std::string settingName, std::string des, std::string* ptr, std::string defaultValue,
		size_t maxLen = 64,
		std::optional<std::function<bool(void)>> _dependOn = std::nullopt) {
		this->name = settingName;
		this->description = des;
		this->value = ptr;
		this->defaultValue = defaultValue;
		this->maxLength = maxLen;
		*this->value = defaultValue;
		this->type = SettingType::TEXT_S;
		this->dependOn = _dependOn;
	}
};
