#pragma once
#include <Windows.h>
#include <cstdio>

namespace Spdlog {
	void init();
	void logImpl(const char* level, const char* msg);

	template<typename... Args>
	void log(const char*, const char*, Args...) {}

	inline void log(const char*, const char*) {}
}

#define LOG_TRACE(...)    ((void)0)
#define LOG_DEBUG(...)    ((void)0)
#define LOG_INFO(...)     ((void)0)
#define LOG_WARN(...)     ((void)0)
#define LOG_ERROR(...)    ((void)0)
#define LOG_CRITICAL(...) ((void)0)
