#include "ImGuiUtils.h"
#include "../Utils/Maths.h"
#include "../Client/Managers/ModuleManager/ModuleManager.h"
#include "../Client/Managers/ModuleManager/Modules/Client/CustomFont.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
    constexpr float kMinBakedTextSize = 6.f;
    constexpr float kMaxBakedTextSize = 128.f;

    float getSafeTextSize(float baseSize, float scale, float textSize) {
        const float value = baseSize * scale * textSize;
        if (!std::isfinite(value)) return 16.f;
        return std::clamp(roundf(value), kMinBakedTextSize, kMaxBakedTextSize);
    }

    ImFont* resolveFont(ImFont* font) {
        if (font) return font;
        if (!ImGui::GetCurrentContext()) return nullptr;
        if (ImFont* overrideFont = ImGuiUtils::getTextFontOverride())
            return overrideFont;
        return ImGui::GetFont();
    }

    bool hasBakedSize(ImFont* font, float size) {
        return font && font->GetFontBaked(size) != nullptr;
    }

	Vec2<float> snapToFramebufferPixel(Vec2<float> position) {
		const ImVec2 framebufferScale = ImGui::GetIO().DisplayFramebufferScale;
		const float scaleX = std::isfinite(framebufferScale.x) && framebufferScale.x > 0.f ? framebufferScale.x : 1.f;
		const float scaleY = std::isfinite(framebufferScale.y) && framebufferScale.y > 0.f ? framebufferScale.y : 1.f;
		return Vec2<float>(roundf(position.x * scaleX) / scaleX, roundf(position.y * scaleY) / scaleY);
	}

	float boldPixelOffset() {
		const float scale = ImGui::GetIO().DisplayFramebufferScale.x;
		return std::isfinite(scale) && scale > 0.f ? 1.f / scale : 1.f;
	}

	float styleExtraWidth(float size) {
		if (ImGuiUtils::getFontStyle() == 1) return boldPixelOffset();
		if (ImGuiUtils::getFontStyle() == 2) return size * 0.25f;
		return 0.f;
	}

	void drawStyledText(ImDrawList* list, ImFont* font, float size, Vec2<float> pos,
		ImU32 color, const char* text) {
		const int style = ImGuiUtils::getFontStyle();
		const float originY = std::trunc(pos.y);
		auto drawPass = [&](float boldOffset) {
			const int firstVertex = list->VtxBuffer.Size;
			list->AddText(font, size, pos.toImVec2(), color, text);
			const float offsetX = pos.x - std::trunc(pos.x) + boldOffset;
			const float offsetY = pos.y - originY;
			for (int quad = firstVertex; quad + 3 < list->VtxBuffer.Size; quad += 4) {
				const float line = (std::max)(0.f, std::floor((list->VtxBuffer[quad].pos.y - originY) / size));
				const float baseline = originY + (line + 1.f) * size;
				for (int corner = 0; corner < 4; ++corner) {
					ImDrawVert& vertex = list->VtxBuffer[quad + corner];
					if (style == 2)
						vertex.pos.x += std::clamp((baseline - vertex.pos.y) * 0.2f, 0.f, size * 0.25f);
					vertex.pos.x += offsetX;
					vertex.pos.y += offsetY;
				}
			}
		};
		drawPass(0.f);
		if (style == 1) drawPass(boldPixelOffset());
	}
}

void ImGuiUtils::setDrawList(ImDrawList* d) {
	drawlist = d;
}

void ImGuiUtils::drawText(Vec2<float> textPos, const std::string& textStr, UIColor color, float textSize, bool glow, float thick_ness) {
	drawText(nullptr, textPos, textStr, color, textSize, glow, thick_ness);
}

void ImGuiUtils::drawTextGlow(Vec2<float> textPos, const std::string& textStr, UIColor color,
	float textSize, float strength, float radius) {
	if (!drawlist || !ImGui::GetCurrentContext() || textStr.empty() || color.a == 0)
		return;

	ImFont* font = resolveFont(nullptr);
	const float effSize = getSafeTextSize(fontSize, fontScale, textSize);
	if (!hasBakedSize(font, effSize)) return;

	strength = std::clamp(strength, 0.f, 1.f);
	radius = std::clamp(radius, 0.75f, 4.5f);
	textPos = snapToFramebufferPixel(textPos);

	const int rings = (std::max)(1, (std::min)(3, static_cast<int>(std::ceil(radius / 1.75f))));
	constexpr float directions[][2] = {
		{ 1.f, 0.f }, { .7071f, .7071f }, { 0.f, 1.f }, { -.7071f, .7071f },
		{ -1.f, 0.f }, { -.7071f, -.7071f }, { 0.f, -1.f }, { .7071f, -.7071f }
	};
	for (int ring = rings; ring >= 1; --ring) {
		const float normalized = static_cast<float>(ring) / static_cast<float>(rings);
		const float distance = radius * (.25f + normalized * .75f);
		const float falloff = (0.045f + (1.f - normalized) * 0.095f) * strength;
		const uint8_t alpha = static_cast<uint8_t>(std::clamp(
			color.a * falloff, 0.f, 255.f));
		if (alpha == 0) continue;
		const UIColor passColor(color.r, color.g, color.b, alpha);
		for (const auto& direction : directions) {
			const Vec2<float> passPos(textPos.x + direction[0] * distance,
				textPos.y + direction[1] * distance);
			drawStyledText(drawlist, font, effSize, passPos,
				passColor.toImColor(), textStr.c_str());
		}
	}
}

ImFont* ImGuiUtils::getInterfaceFont() {
	if (!ImGui::GetCurrentContext()) return nullptr;
	ImGuiIO& io = ImGui::GetIO();
	if (io.FontDefault) return io.FontDefault;
	return ImGui::GetFont();
}

void ImGuiUtils::drawText(ImFont* requestedFont, Vec2<float> textPos, const std::string& textStr,
	UIColor color, float textSize, bool glow, float thick_ness) {
	(void)glow;
	(void)thick_ness;
	if (!drawlist || !ImGui::GetCurrentContext()) return;
	ImFont* font = resolveFont(requestedFont);
	const float effSize = getSafeTextSize(fontSize, fontScale, textSize);
	if (!hasBakedSize(font, effSize)) return;
	textPos = snapToFramebufferPixel(textPos);
	CustomFont* cFontMod = ModuleManager::getModule<CustomFont>();
	if (cFontMod && cFontMod->fontShadow) {
		const float configuredOffset = std::clamp(cFontMod->shadowSize, 0.5f, 5.f);
		const float scaledOffset = (std::max)(0.5f, configuredOffset * fontScale * textSize);
		drawStyledText(drawlist, font, effSize, snapToFramebufferPixel(textPos.add(scaledOffset)),
			ImColor((int)(color.r / 5.f), (int)(color.g / 5.f), (int)(color.b / 5.f), color.a), textStr.c_str());
	}
	drawStyledText(drawlist, font, effSize, textPos, color.toImColor(), textStr.c_str());
}

float ImGuiUtils::getTextWidth(const std::string& textStr, float textSize) {
	return getTextWidth(nullptr, textStr, textSize);
}

float ImGuiUtils::getTextWidth(ImFont* requestedFont, const std::string& textStr, float textSize) {
	if (!ImGui::GetCurrentContext()) return 0.f;
	ImFont* font = resolveFont(requestedFont);
	const float effSize = getSafeTextSize(fontSize, fontScale, textSize);
	if (!hasBakedSize(font, effSize)) return 0.f;
	return font->CalcTextSizeA(effSize, (std::numeric_limits<float>::max)(), -1, textStr.c_str()).x
		+ (textStr.empty() ? 0.f : styleExtraWidth(effSize));
}

float ImGuiUtils::getReadableTextScale(float textSize, float minimumPixels) {
	const float baseSize = fontSize * fontScale;
	if (!std::isfinite(baseSize) || baseSize <= 0.f) return textSize;
	return (std::max)(textSize, minimumPixels / baseSize);
}

float ImGuiUtils::getBakedTextScale(float textSize) {
	if (!ImGui::GetCurrentContext()) return textSize;
	ImFont* font = resolveFont(nullptr);
	const float baseSize = fontSize * fontScale;
	if (!font || !std::isfinite(baseSize) || baseSize <= 0.f) return textSize;
	ImFontBaked* baked = font->GetFontBaked(getSafeTextSize(fontSize, fontScale, textSize));
	return baked ? baked->Size / baseSize : textSize;
}

float ImGuiUtils::getTextHeight(float textSize) {
	return getTextHeight(nullptr, textSize);
}

float ImGuiUtils::getTextHeight(ImFont* requestedFont, float textSize) {
	if (!ImGui::GetCurrentContext()) return 0.f;
	ImFont* font = resolveFont(requestedFont);
	const float effSize = getSafeTextSize(fontSize, fontScale, textSize);
	if (!hasBakedSize(font, effSize)) return 0.f;
	return font->CalcTextSizeA(effSize, (std::numeric_limits<float>::max)(), -1, "").y;
}

void ImGuiUtils::drawRectangle(const Vec4<float>& rectPos, UIColor color, float lineWidth, float rounding, ImDrawFlags flags) {
	drawlist->AddRect(ImVec2(rectPos.x, rectPos.y), ImVec2(rectPos.z, rectPos.w), color.toImColor(), rounding, flags, lineWidth);
}

void ImGuiUtils::drawRectangleShadow(const Vec4<float>& rectPos, UIColor color, float shadow_thickness, const ImVec2& shadow_offset, float rounding, ImDrawFlags flags) {
	if (!drawlist || !ImGui::GetCurrentContext() || shadow_thickness <= 0.f || color.a == 0)
		return;

	const int layers = (std::max)(4, (std::min)(24, static_cast<int>(std::ceil(shadow_thickness * 1.5f))));
	const float baseAlpha = color.a / 255.f;
	for (int layer = layers; layer >= 1; --layer) {
		const float t = static_cast<float>(layer) / static_cast<float>(layers);
		const float expand = shadow_thickness * t;
		const float falloff = 0.018f + (1.f - t) * 0.105f;
		const uint8_t alpha = static_cast<uint8_t>(std::clamp(baseAlpha * falloff * 255.f, 0.f, 255.f));
		if (alpha == 0) continue;

		const ImVec2 min(rectPos.x - expand + shadow_offset.x, rectPos.y - expand + shadow_offset.y);
		const ImVec2 max(rectPos.z + expand + shadow_offset.x, rectPos.w + expand + shadow_offset.y);
		drawlist->AddRect(min, max, ImColor(color.r, color.g, color.b, alpha),
			rounding + expand * .45f, flags,
			(std::max)(0.75f, shadow_thickness / static_cast<float>(layers)));
	}
}

void ImGuiUtils::drawRectangle(Vec2<float> pMin, Vec2<float> pMax, UIColor color, float lineWidth, float rounding, ImDrawFlags flags) {
	drawlist->AddRect(pMin.toImVec2(), pMax.toImVec2(), color.toImColor(), rounding, flags, lineWidth);
}

void ImGuiUtils::fillRectangle(Vec4<float> rectPos, UIColor color, float rounding, ImDrawFlags flags) {
	drawlist->AddRectFilled(ImVec2(rectPos.x, rectPos.y), ImVec2(rectPos.z, rectPos.w), color.toImColor(), rounding, flags);
}

void ImGuiUtils::fillRectangle(Vec2<float> pMin, Vec2<float> pMax, UIColor color, float rounding, ImDrawFlags flags) {
	drawlist->AddRectFilled(pMin.toImVec2(), pMax.toImVec2(), color.toImColor(), rounding, flags);
}
void ImGuiUtils::fillRoundedRectangle(const Vec4<float>& rectPos, UIColor color, float rounding, ImDrawFlags flags) {
	drawlist->AddRectFilled(
		ImVec2(rectPos.x, rectPos.y),
		ImVec2(rectPos.z, rectPos.w),
		color.toImColor(),
		rounding,
		flags
	);
}

void ImGuiUtils::fillRoundedRectangle(Vec2<float> pMin, Vec2<float> pMax, UIColor color, float rounding, ImDrawFlags flags) {
	drawlist->AddRectFilled(
		pMin.toImVec2(),
		pMax.toImVec2(),
		color.toImColor(),
		rounding,
		flags
	);
}

void ImGuiUtils::drawRoundedRectangle(const Vec4<float>& rectPos, UIColor color, float thickness, float rounding, ImDrawFlags flags) {
	drawlist->AddRect(
		ImVec2(rectPos.x, rectPos.y),
		ImVec2(rectPos.z, rectPos.w),
		color.toImColor(),
		rounding,
		flags,
		thickness
	);
}

void ImGuiUtils::drawRoundedRectangle(Vec2<float> pMin, Vec2<float> pMax, UIColor color, float thickness, float rounding, ImDrawFlags flags) {
	drawlist->AddRect(
		pMin.toImVec2(),
		pMax.toImVec2(),
		color.toImColor(),
		rounding,
		flags,
		thickness
	);
}
void ImGuiUtils::fillCircle(const Vec2<float>& center, float radius, const UIColor& color, int num_seg) {
	if (radius <= 0.f) return;
	if (num_seg <= 0)
		num_seg = (int)std::clamp(radius * 0.8f + 6.f, 6.f, 48.f);
	drawlist->AddCircleFilled(ImVec2(center.x, center.y), radius, IM_COL32(color.r, color.g, color.b, color.a), num_seg);
}
void ImGuiUtils::drawCircle(const Vec2<float>& center, float radius, const UIColor& color, float thickness, int num_seg) {
	if (radius <= 0.f) return;
	if (num_seg <= 0)
		num_seg = (int)std::clamp(radius * 0.8f + 6.f, 6.f, 48.f);
	drawlist->AddCircle(ImVec2(center.x, center.y), radius, IM_COL32(color.r, color.g, color.b, color.a), num_seg, thickness);
}
void ImGuiUtils::drawCircle(Vec2<float> center, ImColor color, float radius, float thickness, int num_seg) {
	if (radius <= 0.f) return;
	if (num_seg <= 0)
		num_seg = (int)std::clamp(radius * 0.8f + 6.f, 6.f, 48.f);
	drawlist->AddCircle(ImVec2(center.x, center.y), radius, color, num_seg, thickness);
}
void ImGuiUtils::fillGradientRectangle(const Vec4<float>& rectPos, UIColor colorTopLeft, UIColor colorTopRight, UIColor colorBottomLeft, UIColor colorBottomRight) {
	drawlist->AddRectFilledMultiColor(
		ImVec2(rectPos.x, rectPos.y),
		ImVec2(rectPos.z, rectPos.w),
		IM_COL32(colorTopLeft.r, colorTopLeft.g, colorTopLeft.b, colorTopLeft.a),
		IM_COL32(colorTopRight.r, colorTopRight.g, colorTopRight.b, colorTopRight.a),
		IM_COL32(colorBottomRight.r, colorBottomRight.g, colorBottomRight.b, colorBottomRight.a),
		IM_COL32(colorBottomLeft.r, colorBottomLeft.g, colorBottomLeft.b, colorBottomLeft.a)
	);
}
void ImGuiUtils::drawDashedLine(Vec2<float> start, Vec2<float> end, UIColor color, float thickness, float dashLength, float gapLength) {
	Vec2<float> delta = end.sub(start);
	float len = delta.distance(Vec2<float>(0.f, 0.f));
	if (len <= 0.f) return;
	Vec2<float> dir(delta.x / len, delta.y / len);
	float step = dashLength + gapLength;
	if (step <= 0.f) return;

	float d = 0.f;
	while (d < len) {
		float segEnd = (std::min)(d + dashLength, len);
		float sx = start.x + dir.x * d;
		float sy = start.y + dir.y * d;
		float ex = start.x + dir.x * segEnd;
		float ey = start.y + dir.y * segEnd;
		drawlist->AddLine(ImVec2(sx, sy), ImVec2(ex, ey), color.toImColor(), thickness);
		d += step;
	}
}

void ImGuiUtils::drawMtfFlowText(float x, float y, const std::string& text,
    float textSize, int alpha, double timeSec, float flowSpeed, float shadow) {
    if (text.empty()) return;
    static const int stops[5][3] = {
        { 233, 99, 141 }, { 247, 168, 184 }, { 255, 255, 255 }, { 247, 168, 184 }, { 233, 99, 141 }
    };
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size();) {
        ++count;
        const auto ch = static_cast<unsigned char>(text[i]);
        i += (ch & 0x80) == 0 ? 1 : (ch & 0xE0) == 0xC0 ? 2 : (ch & 0xF0) == 0xE0 ? 3
            : (ch & 0xF8) == 0xF0 ? 4 : 1;
    }
    std::size_t index = 0;
    for (std::size_t i = 0; i < text.size();) {
        const auto ch = static_cast<unsigned char>(text[i]);
        const std::size_t len = (ch & 0x80) == 0 ? 1 : (ch & 0xE0) == 0xC0 ? 2
            : (ch & 0xF0) == 0xE0 ? 3 : (ch & 0xF8) == 0xF0 ? 4 : 1;
        if (i + len > text.size()) break;
        const std::string glyph = text.substr(i, len);
        const float base = count > 1 ? static_cast<float>(index) / static_cast<float>(count - 1) : 0.f;
        float t = base * 0.3f + static_cast<float>(timeSec) * 0.07f * flowSpeed;
        t -= std::floor(t);
        const float seg = t * 4.f;
        const int si = (std::min)(static_cast<int>(seg), 3);
        const float f = seg - static_cast<float>(si);
        const int r = stops[si][0] + static_cast<int>(static_cast<float>(stops[si + 1][0] - stops[si][0]) * f);
        const int g = stops[si][1] + static_cast<int>(static_cast<float>(stops[si + 1][1] - stops[si][1]) * f);
        const int b = stops[si][2] + static_cast<int>(static_cast<float>(stops[si + 1][2] - stops[si][2]) * f);
        if (shadow > 0.f)
            drawText(Vec2<float>(x + shadow, y + shadow), glyph,
                UIColor(0, 0, 0, alpha * 220 / 255), textSize, false);
        drawText(Vec2<float>(x, y), glyph, UIColor(r, g, b, alpha), textSize, false);
        x += getTextWidth(glyph, textSize);
        ++index;
        i += len;
    }
}
