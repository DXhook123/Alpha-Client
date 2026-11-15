#pragma once

#include "Font.h"
#include "../Classes/GuiData.h"
#include "../Render/MinecraftUIRenderContext.h"
#include "../Game.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <string>

class McrRender final {
public:
	static void setUseMinecraftFont(bool enabled) {
		useMinecraftFontFlag.store(enabled, std::memory_order_release);
	}

	[[nodiscard]] static bool useMinecraftFont() {
		return useMinecraftFontFlag.load(std::memory_order_acquire);
	}

	static bool begin(MinecraftUIRenderContext* renderContext) {
		if (!renderContext || !renderContext->vtable) return false;
		context = renderContext;
		nativeContextAvailableFlag.store(true, std::memory_order_release);
		return true;
	}

	static void end() {
		if (context) context->flushText();
		context = nullptr;
	}

	[[nodiscard]] static bool isNativeHudEnabled() {
		return useMinecraftFont() && nativeContextAvailableFlag.load(std::memory_order_acquire);
	}
	[[nodiscard]] static float estimateTextWidth(const std::string& text, float textSize) {
		float width = 0.f;
		const float scale = (std::clamp)(textSize, 0.1f, 8.f);
		for (std::size_t i = 0; i < text.size();) {
			const unsigned char first = static_cast<unsigned char>(text[i]);
			uint32_t codepoint = 0;
			std::size_t length = 1;
			if ((first & 0x80u) == 0) {
				codepoint = first;
			}
			else if ((first & 0xE0u) == 0xC0u && i + 1 < text.size()) {
				codepoint = (first & 0x1Fu) << 6;
				codepoint |= static_cast<unsigned char>(text[i + 1]) & 0x3Fu;
				length = 2;
			}
			else if ((first & 0xF0u) == 0xE0u && i + 2 < text.size()) {
				codepoint = (first & 0x0Fu) << 12;
				codepoint |= (static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 6;
				codepoint |= static_cast<unsigned char>(text[i + 2]) & 0x3Fu;
				length = 3;
			}
			else if ((first & 0xF8u) == 0xF0u && i + 3 < text.size()) {
				codepoint = (first & 0x07u) << 18;
				codepoint |= (static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 12;
				codepoint |= (static_cast<unsigned char>(text[i + 2]) & 0x3Fu) << 6;
				codepoint |= static_cast<unsigned char>(text[i + 3]) & 0x3Fu;
				length = 4;
			}

			if (codepoint == '\n' || codepoint == '\r') {
				++i;
				continue;
			}

			const bool doubleWidth =
				(codepoint >= 0x1100 && codepoint <= 0x11FF) ||
				(codepoint >= 0x2E80 && codepoint <= 0xA4CF) ||
				(codepoint >= 0xAC00 && codepoint <= 0xD7AF) ||
				(codepoint >= 0xF900 && codepoint <= 0xFAFF) ||
				(codepoint >= 0xFF01 && codepoint <= 0xFFEF);
			if (doubleWidth) {
				width += 16.f;
			}
			else if (codepoint == ' ') {
				width += 4.f;
			}
			else if (codepoint == 'i' || codepoint == 'l' || codepoint == 'I' ||
				codepoint == '!' || codepoint == '.' || codepoint == ',' ||
				codepoint == ':' || codepoint == ';' || codepoint == '|') {
				width += 4.f;
			}
			else if (codepoint == 'M' || codepoint == 'W' || codepoint == '@' || codepoint == '#') {
				width += 9.f;
			}
			else {
				width += 8.f;
			}
			i += length;
		}
		return width * scale * 1.2f;
	}

	static void fillRectangle(float left, float top, float right, float bottom,
		const mce::Color& color, float alpha = 1.f) {
		if (!context) return;
		context->fillRectangle({ toUi(left), toUi(right), toUi(top), toUi(bottom) }, color,
			(std::clamp)(alpha, 0.f, 1.f));
	}

	static void drawText(float left, float top, float width, float height,
		const std::string& text, const mce::Color& color, float textSize, bool shadow = false,
		float alpha = 1.f) {
		if (!context || text.empty() || width <= 0.f || height <= 0.f) return;

		TextMeasureData measure{};
		measure.textSize = (std::clamp)(textSize, 0.1f, 8.f);
		measure.displayShadow = shadow;
		measure.showColorSymbols = false;
		CaretMeasureData caret{};

		const float scale = uiScale();
		const float drawWidth = (std::max)(width, estimateTextWidth(text, measure.textSize) + 8.f);
		context->drawDebugText(
			{ left * scale, (left + drawWidth) * scale, top * scale, (top + height) * scale },
			text, color, (std::clamp)(alpha, 0.f, 1.f), 0, measure, caret);
	}

private:
	inline static MinecraftUIRenderContext* context = nullptr;
	inline static std::atomic_bool useMinecraftFontFlag{ true };
	inline static std::atomic_bool nativeContextAvailableFlag{ false };

	[[nodiscard]] static float uiScale() {
		GuiData* gui = nullptr;
		if (context && context->clientInstance)
			gui = context->clientInstance->getGuiData();
		if (!gui) gui = Game::getGuiData();

		const float scale = gui ? gui->invGuiScale : 1.f;
		return std::isfinite(scale) && scale > 0.f && scale < 8.f ? scale : 1.f;
	}

	[[nodiscard]] static float toUi(float value) {
		return value * uiScale();
	}
};
