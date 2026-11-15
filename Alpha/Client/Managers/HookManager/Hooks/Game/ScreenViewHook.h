#pragma once
#include "../FuncHook.h"
#include "sdk/Game.h"
#include "sdk/Core/ClientInstance.h"
#include "sdk/Render/MinecraftUIRenderContext.h"
#include "Client/Managers/ModuleManager/Modules/Client/HudEditor.h"
#include "Client/Managers/ModuleManager/ModuleManager.h"

class ScreenViewHook : public FuncHook {
public:
	static inline ClientInstance* capturedCInst = nullptr;
	static inline ScreenContext* capturedScreenContext = nullptr;

private:
	using func_t = void(__fastcall*)(void*, void*);
	static inline func_t oFunc = nullptr;

	static void __fastcall ScreenViewCallback(void* view, void* ctx) {
		FuncHook::CallbackGuard lifetime;
		ClientInstance* currentCInst = nullptr;
		ScreenContext* currentScreenContext = nullptr;
		if (ctx) {
			const uintptr_t base = reinterpret_cast<uintptr_t>(ctx);
			if (auto* ci = *reinterpret_cast<ClientInstance**>(base + 0x08)) capturedCInst = currentCInst = ci;
			if (auto* sc = *reinterpret_cast<ScreenContext**>(base + 0x10)) capturedScreenContext = currentScreenContext = sc;
		}

		const bool inWorld = Game::getLocalPlayer() != nullptr;
		if (inWorld) {
			if (HudEditor* hud = ModuleManager::getModule<HudEditor>())
				hud->renderNative(reinterpret_cast<MinecraftUIRenderContext*>(ctx));
		}
		oFunc(view, ctx);
	}

public:
	ScreenViewHook() {
		OriginFunc = (void*)&oFunc;
		func = (void*)&ScreenViewCallback;
	}
};
