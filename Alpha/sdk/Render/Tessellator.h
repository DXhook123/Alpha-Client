#pragma once
#include "../../Utils/MemoryUtil.h"
#include "../../Utils/Maths.h"
#include "../GameVersion.h"

enum class VertexFormat : char {
	QUAD          = 1,
	TRIANGLE_LIST = 2,
	LINE_LIST     = 4,
};

struct MeshTag {
	const char* str;
	size_t      len;
};

class Tessellator {
public:

	inline void begin(VertexFormat format, int maxVertices = 0, bool useIndex = false) {
		static uintptr_t addr = MemoryUtil::getGameAddress() + SdkVersion::current().tessBegin;
		using tess_begin_t = void(__thiscall*)(Tessellator*, void*, char, int, uint8_t);
		static tess_begin_t func = reinterpret_cast<tess_begin_t>(addr);
		func(this, nullptr, (char)format, maxVertices, useIndex);
	}

	inline void color(float r, float g, float b, float a) {
		static uintptr_t addr = MemoryUtil::getGameAddress() + SdkVersion::current().tessColor;
		using tess_color_t = void(__thiscall*)(Tessellator*, float, float, float, float);
		static tess_color_t func = reinterpret_cast<tess_color_t>(addr);
		func(this, r, g, b, a);
	}

	inline void vertex(float x, float y, float z) {
		static uintptr_t addr = MemoryUtil::getGameAddress() + SdkVersion::current().tessVertex;
		using tess_vertex_t = void(__thiscall*)(Tessellator*, float, float, float);
		static tess_vertex_t func = reinterpret_cast<tess_vertex_t>(addr);
		func(this, x, y, z);
	}

	inline void render(void* outMesh, int mode, const MeshTag& tag, short flags = 0) {
		static uintptr_t addr = MemoryUtil::getGameAddress() + SdkVersion::current().tessRender;
		using tess_render_t = void(__fastcall*)(Tessellator*, void*, int, const MeshTag*, short);
		static tess_render_t func = reinterpret_cast<tess_render_t>(addr);
		func(this, outMesh, mode, &tag, flags);
	}
private:
	char pad[0x300];
};
