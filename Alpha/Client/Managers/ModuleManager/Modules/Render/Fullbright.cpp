#include "Fullbright.h"
#include "Client/Client.h"

Fullbright::Fullbright() : Module("Fullbright", "Removes darkness by boosting gamma", Category::RENDER) {
	registerSetting(new SliderSetting<int>("Brightness", "Gamma value", &brightness, 10, 1, 20));
}

void Fullbright::onEnable() {
}

void Fullbright::onDisable() {
}
