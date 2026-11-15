#pragma once
#include <string>

#define logF(...) ((void)0);

class Logger {
public:
	static void init();
	static void debugOutput(const char* output, ...);
};
