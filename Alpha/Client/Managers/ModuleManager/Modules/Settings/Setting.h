#pragma once
#include <string>
#include <optional>
#include <functional>
#include "Utils/Maths.h"

enum class SettingType {
	UNKNOW_S,
	BOOL_S,
	KEYBIND_S,
	ENUM_S,
	SLIDER_S,
	TEXT_S,
	COLOR_S
};

class Setting {
public:
	virtual ~Setting() = default;
	std::string name;
	std::string description;
	SettingType type = SettingType::UNKNOW_S;
	std::optional<std::function<bool(void)>> dependOn = std::nullopt;

	float selectedAnim = 0.f;
	bool isDragging = false;
	bool isDragging2 = false;
	bool isDragging3 = false;
	Vec2<float> pos;
	float hueDuration = 0.f;
};
