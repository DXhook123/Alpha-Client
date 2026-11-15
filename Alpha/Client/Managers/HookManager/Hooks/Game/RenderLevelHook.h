#pragma once
#include "../FuncHook.h"
#include "sdk/Game.h"
#include "sdk/Render/ScreenContext.h"
#include "sdk/Render/LevelRenderer.h"
#include "sdk/Core/ClientInstance.h"
#include "../../../ModuleManager/ModuleManager.h"

class RenderLevelHook : public FuncHook {
public:
	static inline ScreenContext* capturedScreenContext = nullptr;

private:
	using func_t = void(__fastcall*)(void*, ScreenContext*, void*);
	static inline func_t oFunc = nullptr;

	static void __fastcall RenderLevelCallback(void* lr, ScreenContext* sc, void* fro) {
		FuncHook::CallbackGuard lifetime;
		if (sc)
			capturedScreenContext = sc;
		oFunc(lr, sc, fro);
	}

public:
	RenderLevelHook() {
		OriginFunc = (void*)&oFunc;
		func = (void*)&RenderLevelCallback;
	}
};
