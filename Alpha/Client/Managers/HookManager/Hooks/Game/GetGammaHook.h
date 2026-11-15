#pragma once
#include "../FuncHook.h"
#include "Client/Managers/ModuleManager/ModuleManager.h"
#include "Client/Managers/ModuleManager/Modules/Render/Fullbright.h"

inline void* gCapturedOptions = nullptr;

class GetGammaHook : public FuncHook {
private:
	using func_t = float(__fastcall*)(void*);
	static inline func_t oFunc = nullptr;

	static float __fastcall GetGammaCallback(void* options) {
		FuncHook::CallbackGuard lifetime;
		gCapturedOptions = options;
		Fullbright* fullbright = ModuleManager::getModule<Fullbright>();
		if (fullbright && fullbright->isEnabled())
			return static_cast<float>(fullbright->brightness);
		return oFunc(options);
	}

public:
	GetGammaHook() {
		OriginFunc = (void*)&oFunc;
		func = (void*)&GetGammaCallback;
	}
};
