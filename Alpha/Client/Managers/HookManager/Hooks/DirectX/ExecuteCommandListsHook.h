#pragma once

#include <d3d12.h>
#include "../FuncHook.h"
#include "PresentHook.h"

class ExecuteCommandListsHook : public FuncHook {
private:
	using func_t = void(__thiscall*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
	static inline func_t oFunc;

	static void callback(ID3D12CommandQueue* queue, UINT numCommandLists, ID3D12CommandList* const* ppCommandLists) {
		PresentHook::CallbackGuard callbackGuard;
		if (!callbackGuard) {
			oFunc(queue, numCommandLists, ppCommandLists);
			return;
		}
		PresentHook::setD3D12CommandQueue(queue);
		oFunc(queue, numCommandLists, ppCommandLists);
	}

public:
	ExecuteCommandListsHook() {
		OriginFunc = (void*)&oFunc;
		func = (void*)&callback;
	}
};
