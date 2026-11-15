#pragma once
#include <string>
#include <memory>
#include <atomic>
#include <cstdint>

class FuncHook {
private:
	static inline std::atomic<uint32_t> activeCallbacks{0};
public:
	class CallbackGuard {
	public:
		CallbackGuard() { activeCallbacks.fetch_add(1, std::memory_order_acquire); }
		~CallbackGuard() {
			if (activeCallbacks.fetch_sub(1, std::memory_order_release) == 1) activeCallbacks.notify_all();
		}
		CallbackGuard(const CallbackGuard&) = delete;
		CallbackGuard& operator=(const CallbackGuard&) = delete;
	};
	static void waitForCallbacks() {
		auto count = activeCallbacks.load(std::memory_order_acquire);
		while (count) {
			activeCallbacks.wait(count, std::memory_order_acquire);
			count = activeCallbacks.load(std::memory_order_acquire);
		}
	}
	const char* name;
	uintptr_t address;
	void* OriginFunc;
	void* func;
	bool enable = false;
	bool optional = false;

	virtual bool enableHook();
	virtual void onHookRequest();

	template <auto T>
	auto getOriginal() {
		return reinterpret_cast<decltype(T)>(OriginFunc);
	}
};
