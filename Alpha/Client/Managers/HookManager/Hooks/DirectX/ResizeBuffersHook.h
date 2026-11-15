#pragma once
#include "../FuncHook.h"
#include <Windows.h>
#include <imfx.h>
#include <dxgi.h>
#include "PresentHook.h"

class ResizeBuffersHook : public FuncHook {
private:
	using reiszeBuffers_t = HRESULT(__thiscall*)(IDXGISwapChain*, int, int, int, DXGI_FORMAT, int);
	static inline reiszeBuffers_t oResizeBuffers;

	static HRESULT resizeBuffersCallback(IDXGISwapChain* swapChain, int bufferCount, int width, int height, DXGI_FORMAT newFormat, int swapChainFlags) {
		PresentHook::CallbackGuard callback;
		if (!callback)
			return oResizeBuffers(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
		PresentHook::invalidateD3D12();
		PresentHook::invalidateD3D11();
		if (ImGui::GetCurrentContext() != nullptr)
			ImFX::CleanupFX();
		return oResizeBuffers(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
	}
public:
	ResizeBuffersHook() {
		OriginFunc = (void*)&oResizeBuffers;
		func = (void*)&resizeBuffersCallback;
	}
};
