#include "Module.h"

#include "Client/Managers/ModuleManager/ModuleManager.h"

#include <utility>

Module::Module(std::string moduleName, std::string des, Category c, int key, int activation) {
	name = std::move(moduleName);
	description = std::move(des);
	category = c;
	keybind = key;
	activationMode = activation;
	if (c == Category::Hud) return;
	this->registerSetting(new KeybindSetting("Keybind", "Toggle module", &keybind, key));
	this->registerSetting(new EnumSetting("Activation", "Choose toggle or hold key activation",
		{ "Toggle", "Hold" }, &activationMode, activation));
	this->registerSetting(new BoolSetting("Drawn", "Draw this module in ArrayList", &moduleDrawn, true));
}

void Module::setEnabled(bool enable) {
	if (enabled == enable)
		return;

	enabled = enable;
	if (enabled)
		onEnable();
	else
		onDisable();

}
