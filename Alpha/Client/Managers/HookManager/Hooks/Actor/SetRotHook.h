#pragma once
#include "../FuncHook.h"
#include "sdk/Game.h"
#include "Client/Managers/ModuleManager/ModuleManager.h"

class ActorSetRotHook : public FuncHook {
private:
    using func_t = __int64(__fastcall*)(float*, uint32_t*, __int64);
    static inline func_t oFunc;

    static __int64 ActorSetRotCallback(float* a1, uint32_t* a2, __int64 a3) {
		FuncHook::CallbackGuard lifetime;
        Actor* actor = reinterpret_cast<Actor*>(a3);
        auto lp = Game::getLocalPlayer();
        if (actor == lp) {
            __int64 result = oFunc(a1, a2, a3);

            ModuleManager::onUpdateRotation(lp);

            return result;
        }

        return oFunc(a1, a2, a3);
    }

public:
    ActorSetRotHook() {
        OriginFunc = (void*)&oFunc;
        func = (void*)&ActorSetRotCallback;
    }
};
