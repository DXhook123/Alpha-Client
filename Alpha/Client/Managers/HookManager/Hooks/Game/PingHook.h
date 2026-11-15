#pragma once
#include "../FuncHook.h"
#include <atomic>

class PingHook : public FuncHook {
private:
	using func_t = int(__fastcall*)(void*, char*);
	static inline func_t oFunc = nullptr;
	static inline std::atomic<int> averagePing = 0;

	static int __fastcall getAveragePingCallback(void* rakPeer, char* guidOrAddress) {
		FuncHook::CallbackGuard lifetime;
		const int ping = oFunc(rakPeer, guidOrAddress);
		if (ping >= 0 && ping < 60000)
			averagePing.store(ping, std::memory_order_relaxed);
		return ping;
	}

public:
	PingHook() {
		name = "PingHook";
		OriginFunc = (void*)&oFunc;
		func = (void*)&getAveragePingCallback;
	}

	static int getAveragePing() {
		return averagePing.load(std::memory_order_relaxed);
	}

	static void reset() {
		averagePing.store(0, std::memory_order_relaxed);
	}
};
