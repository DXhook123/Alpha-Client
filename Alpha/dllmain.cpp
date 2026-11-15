#include "Client/Client.h"
#include "Client/Managers/HookManager/HookManager.h"
#include "Client/Managers/ModuleManager/ModuleManager.h"
#include "Client/Managers/CommandManager/CommandManager.h"
#include <Windows.h>
#include <exception>

static DWORD WINAPI clientThread(LPVOID parameter) {
	const HMODULE module = static_cast<HMODULE>(parameter);
	try {
		if (Client::init()) {
			while (Client::isInitialized()) Sleep(50);
		}
	} catch (const std::exception& e) {
		OutputDebugStringA("[Alpha] init exception: ");
		OutputDebugStringA(e.what());
		OutputDebugStringA("\n");
	} catch (...) {
		OutputDebugStringA("[Alpha] unknown init exception\n");
	}

	Client::requestShutdown();
	HookManager::quiesce();
	Client::shutdown();
	HookManager::shutdown();
	ModuleManager::shutdown();
	CommandManager::shutdown();
	FreeLibraryAndExitThread(module, 0);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(module);
		if (HANDLE thread = CreateThread(nullptr, 0, clientThread, module, 0, nullptr))
			CloseHandle(thread);
	}
	return TRUE;
}
