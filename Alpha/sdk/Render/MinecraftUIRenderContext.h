#pragma once
#include "../Core/ClientInstance.h"
#include "../Core/mce.h"
#include "../GameVersion.h"
#include <optional>
#include <string>
class ScreenContext;
struct RectangleArea {
	float left, right, top, bottom;
	RectangleArea(float l, float r, float t, float b) : left(l), right(r), top(t), bottom(b) {}
};
struct TextMeasureData { float textSize = 1.f; float linePadding = 0.f; bool displayShadow = false; bool showColorSymbols = false; bool hideHyphen = false; };
struct CaretMeasureData { int position = -1; bool shouldRender = false; };
class MinecraftUIRenderContext {
public:
	void** vtable;
	ClientInstance* clientInstance;
	ScreenContext* screenContext;
	void fillRectangle(const RectangleArea& rect, const mce::Color& color, float alpha = 1.f) {
		MemoryUtil::callVirtualFunc<void, const RectangleArea&, const mce::Color&, float>(
			SdkVersion::current().fillRectangle, this, rect, color, alpha);
	}
	void drawDebugText(const RectangleArea& rect, const std::string& text, const mce::Color& color, float alpha,
		int align, const TextMeasureData& measure, const CaretMeasureData& caret) {
		std::string ownedText = text;
		using draw_t = void(__fastcall*)(MinecraftUIRenderContext*, const RectangleArea&,
			std::string&&, const mce::Color&, float, int, const TextMeasureData&, const CaretMeasureData&);
		reinterpret_cast<draw_t>(vtable[SdkVersion::current().drawDebugText])(
			this, rect, std::move(ownedText), color, alpha, align, measure, caret);
	}
	void flushText(float delta = 0.f) {
		if (SdkVersion::is12651())
			MemoryUtil::callVirtualFunc<void, float, std::optional<float>>(
				SdkVersion::current().flushText, this, delta, std::nullopt);
		else
			MemoryUtil::callVirtualFunc<void, float>(SdkVersion::current().flushText, this, delta);
	}
};
static_assert(offsetof(MinecraftUIRenderContext, clientInstance) == 8);
