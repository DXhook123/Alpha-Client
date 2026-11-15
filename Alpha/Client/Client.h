#pragma once
#include <Windows.h>
#include <atomic>
#include <string>

class Client {
private:
	static inline std::atomic_bool initialized{ false };
	static inline std::atomic_bool shuttingDown{ false };
	static inline std::atomic_bool featuresInitialized{ false };
public:
	static inline bool isInitialized() { return initialized.load(std::memory_order_acquire); }
	static bool init();
	static void shutdown();
	static void requestShutdown();
	static void displayMessage(const std::string& message);
	static void displayMessageF(const char* fmt, ...);
};
