#pragma once
#include "../../Utils/MemoryUtil.h"
#include <string>
class MinecraftGame {
public:
	CLASS_MEMBER(std::string, xuid, 0x250);
	CLASS_MEMBER(bool, mouseGrabbed, 0x1E0);
	CLASS_MEMBER(void*, gameRenderer, 0xFA0);
	CLASS_MEMBER(void*, gameRendererLatite, 0x1318);
};
