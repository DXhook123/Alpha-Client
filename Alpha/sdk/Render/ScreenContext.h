#pragma once
#include "../../Utils/MemoryUtil.h"
#include "ShaderColor.h"
#include "Tessellator.h"

class ScreenContext {
public:
	CLASS_MEMBER(ShaderColor*, currentShaderColor, 0x30);
	CLASS_MEMBER(Tessellator*, tessellator, 0xB8);
private:
	char pad[0xD0];
};
