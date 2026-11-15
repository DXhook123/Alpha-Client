#pragma once

#include <string>
#include <vector>
#include <imgui.h>
#include "Settings/Setting.h"
#include "Settings/BoolSetting.h"
#include "Settings/SliderSetting.h"
#include "Settings/EnumSetting.h"
#include "Settings/TextSetting.h"
#include "Settings/KeybindSetting.h"
#include "Settings/ColorSetting.h"
#include "sdk/Game.h"

enum class Category {
	COMBAT = 0,
	MOVEMENT,
	RENDER,
	PLAYER,
	WORLD,
	MISC,
	CLIENT,
	Hud
};

class Packet;

class Module {
private:
	std::string name;
	std::string description;
	Category category;
	bool enabled = false;
	int tickPriority = 0;

protected:
	int keybind = 0;
	int activationMode = 0;

private:
	std::vector<Setting*> settings;

protected:
	inline Setting* registerSetting(Setting* setting) {
		this->settings.push_back(setting);
		return setting;
	}

public:
	float selectedAnim = 0.f;
	float enabledAnim = 0.f;
	bool extended = false;
	bool moduleDrawn = true;

	Module(std::string moduleName, std::string des, Category c, int key = 0, int activation = 0);
	virtual ~Module() = default;

	const std::string& getModuleName() const { return name; }
	const std::string& getDescription() const { return description; }
	virtual std::string getModeText() { return {}; }
	Category getCategory() const { return category; }
	int getKeybind() const { return keybind; }
	void setKeybind(int key) { keybind = key; }
	bool usesHoldActivation() const { return activationMode == 1; }
	bool isHiddenFromArrayList() const { return !moduleDrawn; }
	void setDrawn(bool drawn) { moduleDrawn = drawn; }
	int getTickPriority() const { return tickPriority; }
	void setTickPriority(int priority) { tickPriority = priority; }

	std::vector<Setting*>& getSettingList() { return settings; }

	virtual bool runOnBackground() { return false; }
	virtual bool isEnabled() const { return enabled; }
	virtual void setEnabled(bool enable);
	virtual void toggle() { setEnabled(!enabled); }

	virtual void onEnable() {}
	virtual void onDisable() {}
	virtual void onKeyUpdate(int key, bool isDown) {}
	virtual void onImGuiRender(ImDrawList* drawlist) {}
	virtual void onNormalTick(LocalPlayer* localPlayer) {}
	virtual void onUpdateRotation(LocalPlayer* localPlayer) {}
	virtual void onSendPacket(Packet* packet, bool& cancel) {}
	virtual void onPacketSent(Packet* packet) {}
	virtual void onReceivePacket(Packet* packet, bool& cancel) {}
	virtual void onReceivePacketPost(Packet* packet) {}
	virtual void onLevelRender() {}
};

#define ADD_BOOL(n, d, p, v)                  registerSetting(new BoolSetting(n, d, p, v))
#define ADD_KEYBIND(n, d, p, v)               registerSetting(new KeybindSetting(n, d, p, v))
#define ADD_SLIDER_F(n, d, p, v, a, b)        registerSetting(new SliderSetting<float>(n, d, p, v, a, b))
#define ADD_SLIDER_I(n, d, p, v, a, b)        registerSetting(new SliderSetting<int>(n, d, p, v, a, b))
#define ADD_ENUM(n, d, l, p, v)               registerSetting(new EnumSetting(n, d, l, p, v))
#define ADD_TEXT(n, d, p, v, ...)             registerSetting(new TextSetting(n, d, p, v, __VA_ARGS__))
#define ADD_COLOR(n, d, p, v)                 registerSetting(new ColorSetting(n, d, p, v))

#define ADD_BOOL_DEP(n, d, p, v, dep)         registerSetting(new BoolSetting(n, d, p, v, dep))
#define ADD_KEYBIND_DEP(n, d, p, v, dep)      registerSetting(new KeybindSetting(n, d, p, v, dep))
#define ADD_SLIDER_F_DEP(n, d, p, v, a, b, dep) registerSetting(new SliderSetting<float>(n, d, p, v, a, b, dep))
#define ADD_SLIDER_I_DEP(n, d, p, v, a, b, dep) registerSetting(new SliderSetting<int>(n, d, p, v, a, b, dep))
#define ADD_ENUM_DEP(n, d, l, p, v, dep)      registerSetting(new EnumSetting(n, d, l, p, v, dep))
#define ADD_COLOR_DEP(n, d, p, v, dep)        registerSetting(new ColorSetting(n, d, p, v, dep))
