#include "HookManager.h"
#include <kiero.h>
#include "Hooks/DirectX/ExecuteCommandListsHook.h"
#include "Hooks/DirectX/PresentHook.h"
#include "Hooks/DirectX/ResizeBuffersHook.h"
#include "Hooks/Game/ClientInstanceUpdateHook.h"
#include "Hooks/Game/KeyMouseHook.h"
#include "Hooks/Game/PacketSendHook.h"
#include "Hooks/Game/PacketReceiveHook.h"
#include "Hooks/Game/RenderLevelHook.h"
#include "Hooks/Game/ScreenViewHook.h"
#include "Hooks/Game/PingHook.h"
#include "Hooks/Game/GetGammaHook.h"
#include "Hooks/Actor/NormalTickHook.h"
#include "Hooks/Actor/SetRotHook.h"
#include "sdk/Game.h"
#include "sdk/Signatures.h"
#include "sdk/Core/ClientInstance.h"
#include "sdk/Classes/LoopbackPacketSender.h"
#include "Client/Client.h"
#include <Windows.h>

bool HookManager::initRenderHooks() {
	if (renderHooksInitialized)
		return true;

	const MH_STATUS mhStatus = MH_Initialize();
	if (mhStatus != MH_OK && mhStatus != MH_ERROR_ALREADY_INITIALIZED) {
		LOG_ERROR("[Hook] MinHook initialization failed: %d", (int)mhStatus);
		return false;
	}

	if (!kieroInitialized) {
		if (kiero::init(kiero::RenderType::D3D12) == kiero::Status::Success) {
			isDx11 = false;
		}
		else if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success) {
			isDx11 = true;
		}
		else {
			LOG_ERROR("[Hook] Could not initialize D3D11 or D3D12 method table");
			return false;
		}
		kieroInitialized = true;
	}

	uintptr_t** methodsTable = (uintptr_t**)kiero::getMethodsTable();
	if (!methodsTable) {
		LOG_ERROR("[Hook] Kiero returned a null method table");
		return false;
	}

	if (!isDx11)
		RequestHook<ExecuteCommandListsHook>(methodsTable, 54);
	if (isDx11) {
		RequestHook<PresentHook>(methodsTable, 8);
		RequestHook<ResizeBuffersHook>(methodsTable, 13);
	} else {
		RequestHook<PresentHook>(methodsTable, 140);
		RequestHook<ResizeBuffersHook>(methodsTable, 145);
	}

	bool hooksQueued = true;
	for (auto* funcHook : hooksCache) {
		if (!funcHook->enableHook()) {
			LOG_ERROR("[Hook] ENABLE FAILED: %s @ 0x%p", funcHook->name,
				(void*)funcHook->address);
			hooksQueued = false;
		}
	}
	if (MH_ApplyQueued() != MH_OK) {
		LOG_ERROR("[Hook] Failed to apply render hooks");
		hooksQueued = false;
	}
	for (auto* funcHook : hooksCache)
		delete funcHook;
	hooksCache.clear();
	if (!hooksQueued)
		return false;

	renderHooksInitialized = true;
	return true;
}

bool HookManager::initGameHooks() {
	if (gameHooksInitialized)
		return true;
	if (!renderHooksInitialized || !kieroInitialized)
		return false;

	RequestHook<ClientInstanceUpdateHook>(MemoryUtil::findSignature(Sigs::hook::clientInstanceUpdate));
	ClientInstanceUpdateHook* clientInstanceHook = getHook<ClientInstanceUpdateHook>();
	if (!clientInstanceHook || !clientInstanceHook->enableHook() || MH_ApplyQueued() != MH_OK) {
		LOG_ERROR("[Hook] ClientInstanceUpdate hook could not be installed");
		for (auto* funcHook : hooksCache) delete funcHook;
		hooksCache.clear();
		return false;
	}
	RequestHook<KeyMouseHook>(MemoryUtil::findSignature(Sigs::hook::keyMouse));
	RequestHook<PingHook>(Sigs::hook::rakPeerGetAveragePing);
	uintptr_t** actorVTable = (uintptr_t**)(uintptr_t)MemoryUtil::getVtableFromSig(Sigs::vtable::ActorVatble);
	if (actorVTable) RequestHook<ActorNormalTickHook>(actorVTable, 24);
	else LOG_ERROR("[Hook] FAILED: Actor vtable not found");

	const auto instanceDeadline = GetTickCount64() + 15000;
	while (Game::clientInstance == nullptr) {
		if (!Client::isInitialized() || GetTickCount64() >= instanceDeadline) {
			for (auto* funcHook : hooksCache) delete funcHook;
			hooksCache.clear();
			return false;
		}
		Sleep(10);
	}
	if (PacketSender* sender = Game::clientInstance->getLoopbackPacketSender()) {
		uintptr_t** vtable = *reinterpret_cast<uintptr_t***>(sender);
		RequestHook<PacketSendHook>(vtable, 4);
	}
	RequestHook<GetGammaHook>(Sigs::hook::getGamma);
	RequestHook<ActorSetRotHook>(Sigs::hook::actorSetRot);
	RequestHook<RenderLevelHook>(Sigs::render::levelRendererRenderLevel);
	{
		const uintptr_t callSite = MemoryUtil::findSignature(Sigs::render::screenViewSetupAndRender);
		const uintptr_t screenView = callSite ? MemoryUtil::getFuncFromCall(callSite) : 0;
		if (screenView) RequestHook<ScreenViewHook>(screenView);
		else LOG_WARN("[Hook] ScreenView::setupAndRender signature not found");
	}
	if (const uintptr_t packetFactory = MemoryUtil::findSignature(Sigs::packet::minecraftPacketsCreatePacket))
		PacketReceiveHook::cacheCreatePacketAddress(packetFactory);

	bool hooksQueued = true;
	for (auto* funcHook : hooksCache) {
		if (funcHook == clientInstanceHook) continue;
		if (!funcHook->enableHook()) {
			LOG_ERROR("[Hook] ENABLE FAILED: %s @ 0x%p", funcHook->name, (void*)funcHook->address);
			if (!funcHook->optional) hooksQueued = false;
		}
	}
	if (MH_ApplyQueued() != MH_OK) {
		LOG_ERROR("[Hook] Failed to apply queued hooks");
		hooksQueued = false;
	}
	for (auto* funcHook : hooksCache)
		delete funcHook;
	hooksCache.clear();

	PacketReceiveHook::init();
	if (!hooksQueued)
		return false;
	gameHooksInitialized = true;
	return true;
}

void HookManager::quiesce() {
	PresentHook::beginShutdown();
	MH_DisableHook(MH_ALL_HOOKS);
	FuncHook::waitForCallbacks();
	PresentHook::waitForCallbacks();
}

void HookManager::shutdown() {
	quiesce();
	PresentHook::shutdown();
	PacketReceiveHook::shutdown();
	kiero::shutdown();
	MH_RemoveHook(MH_ALL_HOOKS);
	MH_Uninitialize();
	for (auto& h : hooksCache) delete h;
	hooksCache.clear();
	renderHooksInitialized = false;
	gameHooksInitialized = false;
	kieroInitialized = false;
	isDx11 = false;
}
