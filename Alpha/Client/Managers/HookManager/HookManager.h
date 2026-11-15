#pragma once
#include <string>
#include <vector>
#include <typeinfo>
#include "Hooks/FuncHook.h"
#include "Utils/MemoryUtil.h"
#include "Utils/Spdlog.h"
#include <MinHook.h>

class HookManager {
public:
	static bool initRenderHooks();
	static bool initGameHooks();
	static void quiesce();
	static void shutdown();
private:
	static inline std::vector<FuncHook*> hooksCache;
	static inline bool renderHooksInitialized = false;
	static inline bool gameHooksInitialized = false;
	static inline bool kieroInitialized = false;
	static inline bool isDx11 = false;

	template<typename THook>
	static void RequestHook(uintptr_t address) {
		if (!address) {
			LOG_ERROR("[Hook] FAILED: %s (sig not found)", typeid(THook).name());
			return;
		}
		THook* funcHook = new THook();
		funcHook->name = typeid(THook).name();
		funcHook->address = address;
		funcHook->onHookRequest();
		hooksCache.push_back((FuncHook*)funcHook);
	}

	template<typename THook>
	static void RequestOptionalHook(uintptr_t address) {
		if (!address) {
			LOG_ERROR("[Hook] FAILED: %s (address not found)", typeid(THook).name());
			return;
		}
		THook* funcHook = new THook();
		funcHook->name = typeid(THook).name();
		funcHook->address = address;
		funcHook->optional = true;
		funcHook->onHookRequest();
		hooksCache.push_back((FuncHook*)funcHook);
	}

	template<typename THook>
	static void RequestHook(std::string_view sig) {
		uintptr_t addr = MemoryUtil::findSignature(sig);
		RequestHook<THook>(addr);
	}

	template<typename Hook>
	static void RequestRefHook(std::string_view sig) {
		RequestHook<Hook>(MemoryUtil::resolveRef(MemoryUtil::findSignature(sig)));
	}

	template<typename Hook>
	static void RequestHook(uintptr_t** VTable, int index) {
		if (!VTable || index < 0 || !VTable[index]) {
			LOG_ERROR("[Hook] FAILED: %s (vtable slot invalid: %d)", typeid(Hook).name(), index);
			return;
		}
		RequestHook<Hook>((uintptr_t)VTable[index]);
	}

	template<typename TRet>
	static TRet* getHook() {
		for (FuncHook* funcHook : hooksCache) {
			TRet* result = dynamic_cast<TRet*>(funcHook);
			if (result == nullptr)
				continue;
			return result;
		}
		return nullptr;
	}
};
