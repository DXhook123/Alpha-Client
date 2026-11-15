#pragma once
#include "../../Utils/MemoryUtil.h"
#include "../../Utils/Maths.h"

class LevelRendererPlayer {
public:
	float* getCameraPosPtr() {
		return reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0x710);
	}
	float* getViewMatrix() {
		return reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0xFB0);
	}
	float* getProjMatrix() {
		return reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0xFF0);
	}
	float* getFovXPtr() {
		return reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0xFF0);
	}
	float* getFovYPtr() {
		return reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0x1004);
	}

	float& getFovX() {
		return *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0xF58);
	}

	float& getFovY() {
		return *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0xF6C);
	}
};

class LevelRenderer {
public:
	CLASS_MEMBER(LevelRendererPlayer*, renderPlayer, 0x468);
private:
	char pad[0x500];
};
