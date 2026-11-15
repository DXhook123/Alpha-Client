#pragma once
#include "../FuncHook.h"
#include "Utils/ImGuiInput.h"
#include "Client/Managers/ModuleManager/ModuleManager.h"
#include "Client/Managers/ModuleManager/Modules/Client/ClickGui.h"
#include "Client/Managers/ModuleManager/Modules/Client/HudEditor.h"

class KeyMouseHook : public FuncHook {
private:
	using func_t = void* (__fastcall*)(void*, char, int, int16_t, int16_t, int16_t, int16_t, uint8_t);
	static inline func_t oFunc;

	static void* __fastcall KeyMouseCallback(void* instance, char mouseButton, int buttonData,
		int16_t mouseX, int16_t mouseY, int16_t relX, int16_t relY, uint8_t forceMotionlessPointer) {
		FuncHook::CallbackGuard lifetime;
		if (forceMotionlessPointer == 2)
			forceMotionlessPointer = 1;

		ClickGui* clickGui = ModuleManager::getModule<ClickGui>();
		HudEditor* hudEditor = ModuleManager::getModule<HudEditor>();
		const bool overlayOpen = (clickGui && clickGui->isEnabled()) || (hudEditor && hudEditor->isEnabled());
		if (overlayOpen) {
			if (mouseButton == 4) {
				const float wheelStep = static_cast<float>(static_cast<signed char>(buttonData)) / 120.f;
				ImGuiInput::queueMouseWheel(0.f, wheelStep);
			}
			return instance;
		}
		return oFunc(instance, mouseButton, buttonData, mouseX, mouseY, relX, relY, forceMotionlessPointer);
	}

public:
	KeyMouseHook() {
		name = "KeyMouseHook";
		OriginFunc = (void*)&oFunc;
		func = (void*)&KeyMouseCallback;
	}
};
