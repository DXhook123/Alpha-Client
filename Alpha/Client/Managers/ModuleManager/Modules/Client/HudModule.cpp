#include "ColorModule.h"
#include "HudModule.h"
#include "HudEditor.h"
#include "ClickGui.h"
#include "../../ModuleManager.h"
#include "Renderer/ImGuiUtils.h"
#include "Utils/FileUtil.h"
#include "sdk/Core/mce.h"
#include "sdk/font/McrRender.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

static Module* g_dragOwner = nullptr;

namespace {
	Vec2<float> arrangeDroppedHud(HudModule* owner, const Vec4<float>& oldRect,
		Vec2<float> desired, const Vec2<float>& screen) {
		constexpr float gap = 2.f;
		constexpr float snapDistance = 6.f;
		const float width = oldRect.z - oldRect.x;
		const float height = oldRect.w - oldRect.y;
		std::vector<Vec4<float>> obstacles;
		if (auto* editor = ModuleManager::getModule<HudEditor>()) {
			for (auto* element : editor->getElements()) {
				if (element != owner && element->getShow() && element->editorRectReady)
					obstacles.push_back(element->editorRect);
			}
		}
		float nearestX = snapDistance + 1.f;
		float nearestY = snapDistance + 1.f;
		Vec2<float> snapped = desired;
		for (const auto& other : obstacles) {
			if (desired.y + height >= other.y - 24.f && desired.y <= other.w + 24.f) {
				for (float x : { other.x, other.z - width, other.x - width - gap, other.z + gap }) {
					const float distance = std::abs(x - desired.x);
					if (distance <= snapDistance && distance < nearestX) {
						nearestX = distance;
						snapped.x = x;
					}
				}
			}
			if (desired.x + width >= other.x - 24.f && desired.x <= other.z + 24.f) {
				for (float y : { other.y, other.w - height, other.y - height - gap, other.w + gap }) {
					const float distance = std::abs(y - desired.y);
					if (distance <= snapDistance && distance < nearestY) {
						nearestY = distance;
						snapped.y = y;
					}
				}
			}
		}
		snapped.x = std::clamp(snapped.x, 0.f, (std::max)(0.f, screen.x - width));
		snapped.y = std::clamp(snapped.y, 0.f, (std::max)(0.f, screen.y - height));
		auto clear = [&](Vec2<float> candidate) {
			if (candidate.x < 0.f || candidate.y < 0.f ||
				candidate.x > (std::max)(0.f, screen.x - width) ||
				candidate.y > (std::max)(0.f, screen.y - height)) return false;
			for (const auto& other : obstacles) {
				if (candidate.x < other.z + gap && candidate.x + width > other.x - gap &&
					candidate.y < other.w + gap && candidate.y + height > other.y - gap) return false;
			}
			return true;
		};
		if (clear(snapped)) return snapped;
		Vec2<float> best(oldRect.x, oldRect.y);
		float bestDistance = INFINITY;
		for (const auto& other : obstacles) {
			if (snapped.x >= other.z + gap || snapped.x + width <= other.x - gap ||
			    snapped.y >= other.w + gap || snapped.y + height <= other.y - gap) continue;
			const Vec2<float> candidates[] = {
			    {other.x - gap - width, snapped.y}, {other.z + gap, snapped.y},
			    {snapped.x, other.y - gap - height}, {snapped.x, other.w + gap}
			};
			for (const auto& candidate : candidates) {
			    const float dx = candidate.x - snapped.x;
			    const float dy = candidate.y - snapped.y;
			    const float distance = dx * dx + dy * dy;
			    if (distance < bestDistance && clear(candidate)) {
			        best = candidate;
			        bestDistance = distance;
			    }
			}
		}
		return best;
	}
}

HudModule::HudModule(std::string name, std::string description, Category category)
	: Module(std::move(name), std::move(description), category) {
}

HudModule::~HudModule() {
	resetEditorInteraction();
	for (Setting* setting : getSettingList()) delete setting;
}

void HudModule::initializeSettings(float* scale, bool color) {
	if (scale)
		registerSetting(new SliderSetting<float>("Size", "Scale of this HUD element", scale, *scale, 0.5f, 4.f));
	else
		registerSetting(new SliderSetting<int>("Size", "Text size of this HUD element", &hudFontSize, hudFontSize, 12, 100));
	if (color) {
		registerSetting(new ColorSetting("Color", "Color in Custom mode; dynamic palettes are controlled by Color", &labelColor, labelColor,
            [] { return !ColorModule::hasDynamicColors(); }));
		registerSetting(new SliderSetting<int>("Opacity", "Background opacity", &opacity, opacity, 0, 255));
	}
}

void HudModule::resetEditorInteraction() {
	isDragging = false;
	if (g_dragOwner == this) g_dragOwner = nullptr;
}

bool HudModule::isEditing() {
	HudEditor* editor = ModuleManager::getModule<HudEditor>();
	return editor != nullptr && editor->isEnabled();
}

void HudModule::invalidateNativeText() {
	std::scoped_lock lock(nativeTextMutex);
	nativeTextReady = false;
	nativeTextItems.clear();
}

UIColor HudModule::getHudColor(const UIColor& custom,double offset) const {
    return ColorModule::resolveColor(custom,offset);
}

float HudModule::hudTextWidth(const std::string& text, float scale) const {
	if (McrRender::useMinecraftFont())
		return McrRender::estimateTextWidth(text, scale * ImGuiUtils::getFontScale());
	return ImGuiUtils::getTextWidth(text, scale);
}

void HudModule::drawHudText(const Vec2<float>& pos, const std::string& text, const UIColor& color, float scale) {
	if (McrRender::isNativeHudEnabled()) {
		std::scoped_lock lock(nativeTextMutex);
		nativeTextItems.push_back({ pos, text, color, scale * ImGuiUtils::getFontScale(), ImGuiUtils::getTextHeight(scale) });
	} else {
		ImGuiUtils::drawText(pos, text, color, scale, false);
	}
}

Vec2<float> HudModule::getClampedRenderPosition(float width, float height) {
	if (!ImGui::GetCurrentContext()) return position;
	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	if (!std::isfinite(displaySize.x) || !std::isfinite(displaySize.y) ||
		displaySize.x <= 1.f || displaySize.y <= 1.f)
		return position;

	Vec2<float> result = position;
	if (reflowFontLayout && !isDragging && defaultBottomRow < 0) {
		Vec2<float> topLeft(fontLayoutRect.x, fontLayoutRect.y);
		if (displaySize.x - fontLayoutRect.z <= 12.f)
			topLeft.x = fontLayoutRect.z - width;
		if (displaySize.y - fontLayoutRect.w <= 12.f)
			topLeft.y = fontLayoutRect.w - height;
		if (fontLayoutReference && fontLayoutReference->editorRectReady) {
			const auto& reference = fontLayoutReference->editorRect;
			if (fontLayoutDirection <= 2) {
				topLeft.x = fontLayoutAlignEnd ? reference.z - width : reference.x;
				topLeft.y = fontLayoutDirection == 1
					? reference.y - fontLayoutGap - height : reference.w + fontLayoutGap;
			} else {
				topLeft.y = fontLayoutAlignEnd ? reference.w - height : reference.y;
				topLeft.x = fontLayoutDirection == 3
					? reference.x - fontLayoutGap - width : reference.z + fontLayoutGap;
			}
		}
		result = Vec2<float>(anchoredRight ? topLeft.x + width : topLeft.x, topLeft.y);
	}
	if (defaultBottomRow >= 0) {
		const float rowHeight = ImGuiUtils::getTextHeight(static_cast<float>(hudFontSize) / 25.f);
		result.x = displaySize.x - 6.f;
		result.y = displaySize.y - 6.f - height - (3 - defaultBottomRow) * (rowHeight + 2.f);
	}
	if (anchoredRight) {
		const float minimumAnchor = (std::max)(0.f, width);
		result.x = minimumAnchor >= displaySize.x
			? displaySize.x
			: std::clamp(result.x, minimumAnchor, displaySize.x);
	}
	else
		result.x = std::clamp(result.x, 0.f, (std::max)(0.f, displaySize.x - width));
	result.y = std::clamp(result.y, 0.f, (std::max)(0.f, displaySize.y - height));
	if (reflowFontLayout && !isDragging && defaultBottomRow < 0)
		position = result;
	return result;
}

void HudModule::renderNativeExtras() {
	std::scoped_lock lock(nativeTextMutex);
	for (const NativeTextItem& item : nativeTextItems) {
		const float width = McrRender::estimateTextWidth(item.text, item.scale);
		const mce::Color color(item.color.r / 255.f, item.color.g / 255.f,
			item.color.b / 255.f, item.color.a / 255.f);
		McrRender::drawText(roundf(item.pos.x), roundf(item.pos.y), width, item.height,
			item.text, color, item.scale, false, 1.f);
	}
}

void HudModule::drawLabeledRect(const std::string& label, const std::string& value) {
    const UIColor effectiveLabelColor=getHudColor(labelColor);
	float textSize = (float)hudFontSize / 25.f;
	float textPaddingX = (std::max)(1.f, ((float)spacing + 3.f)) * textSize;
	float textPaddingY = (float)spacing * textSize;
	float textHeight = ImGuiUtils::getTextHeight(textSize);

	std::string labelTrim = label;
	while (!labelTrim.empty() && (labelTrim.back() == ':' || labelTrim.back() == ' '))
		labelTrim.pop_back();

	const bool nativeFont = McrRender::useMinecraftFont();
	const float nativeSize = textSize * ImGuiUtils::getFontScale();
	float labelW = ImGuiUtils::getTextWidth(labelTrim, textSize);
	float labelFullW = ImGuiUtils::getTextWidth(label, textSize);
	float valueW = ImGuiUtils::getTextWidth(value, textSize);
	float sepW = ImGuiUtils::getTextWidth(" : ", textSize);
	if (nativeFont) {
		const float nativeLabelSafety = (std::max)(4.f, 6.f * textSize);
		labelW = McrRender::estimateTextWidth(labelTrim, nativeSize) + nativeLabelSafety;
		labelFullW = McrRender::estimateTextWidth(label, nativeSize) + nativeLabelSafety;
		valueW = McrRender::estimateTextWidth(value, nativeSize);
		sepW = McrRender::estimateTextWidth(" : ", nativeSize);
	}
	const float valueGap = (std::max)(6.f, 6.f * textSize);
	float totalW_normal = labelFullW + valueGap + valueW;
	const float nativeLabelGap = nativeFont ? (std::max)(4.f, 6.f * textSize) : 0.f;
	totalW_normal += nativeLabelGap;

	float rectW = totalW_normal + textPaddingX * 2.f;
	float rectH = textHeight + textPaddingY * 2.f;

	const Vec2<float> renderPosition = getClampedRenderPosition(rectW, rectH);

	Vec4<float> rect;
	if (!anchoredRight) {
		rect = Vec4<float>(renderPosition.x, renderPosition.y, renderPosition.x + rectW, renderPosition.y + rectH);
	}
	else {
		rect = Vec4<float>(renderPosition.x - rectW, renderPosition.y, renderPosition.x, renderPosition.y + rectH);
	}

	{
		const float startX = rect.x + textPaddingX;
		const Vec2<float> labelPos(startX, rect.y + textPaddingY);
		const Vec2<float> valuePos(startX + labelFullW + nativeLabelGap + valueGap, rect.y + textPaddingY);
		const float labelWidth = labelFullW;
		std::scoped_lock lock(nativeTextMutex);
		nativeLabel = label;
		nativeLabelTrim = labelTrim;
		nativeValue = value;
		nativeRect = rect;
		nativeTextHeight = textHeight;
		nativeTextSize = nativeSize;
		nativeOpacity = opacity;
		nativeLabelColor = effectiveLabelColor;
		nativeAnchoredRight = false;
		nativeValueWidth = valueW;
		nativeSeparatorWidth = sepW;
		nativeLabelPos = labelPos;
		nativeValuePos = valuePos;
		nativeLabelWidth = labelWidth;
		nativeTextReady = true;
	}

	if (!McrRender::isNativeHudEnabled()) {
		ImGuiUtils::fillRectangle(rect, UIColor(0, 0, 0, (uint8_t)opacity), 2.f);
		const UIColor valueColor=getHudColor(UIColor(255,255,255,255));
		ImGuiUtils::drawTextGlow(nativeLabelPos, label, effectiveLabelColor, textSize, .95f, 3.5f);
		ImGuiUtils::drawTextGlow(nativeValuePos, value, valueColor, textSize, .95f, 3.5f);
		const uint8_t labelShadowAlpha = static_cast<uint8_t>(std::clamp(effectiveLabelColor.a * .9f, 0.f, 255.f));
		ImGuiUtils::drawText(Vec2<float>(nativeLabelPos.x + 1.25f, nativeLabelPos.y + 1.25f), label,
			UIColor(static_cast<uint8_t>(effectiveLabelColor.r * .03f), static_cast<uint8_t>(effectiveLabelColor.g * .03f),
				static_cast<uint8_t>(effectiveLabelColor.b * .03f), labelShadowAlpha), textSize, false);
		ImGuiUtils::drawText(Vec2<float>(nativeValuePos.x + 1.25f, nativeValuePos.y + 1.25f), value,
			UIColor(8, 8, 8, 230), textSize, false);
		ImGuiUtils::drawText(nativeLabelPos, label, effectiveLabelColor, textSize, false);
		ImGuiUtils::drawText(nativeValuePos, value, valueColor, textSize, false);
	}

	drawEditorRect(rect);
}

void HudModule::renderNative() {
	std::scoped_lock lock(nativeTextMutex);
	if (!nativeTextReady) return;

	McrRender::fillRectangle(nativeRect.x, nativeRect.y, nativeRect.z, nativeRect.w,
		mce::Color::Black, static_cast<float>((std::clamp)(nativeOpacity, 0, 255)) / 255.f);

	const float textSize = nativeTextSize;
	const mce::Color labelMce(
		static_cast<float>(nativeLabelColor.r) / 255.f,
		static_cast<float>(nativeLabelColor.g) / 255.f,
		static_cast<float>(nativeLabelColor.b) / 255.f,
		static_cast<float>(nativeLabelColor.a) / 255.f);
	const UIColor valueColor=getHudColor(UIColor(255,255,255,255));
	const mce::Color valueMce(valueColor.r/255.f,valueColor.g/255.f,valueColor.b/255.f,1.f);
	const float textAlpha = static_cast<float>((std::clamp)(static_cast<int>(nativeLabelColor.a), 0, 255)) / 255.f;

	if (!nativeAnchoredRight) {
		McrRender::drawText(roundf(nativeLabelPos.x), roundf(nativeLabelPos.y), nativeLabelWidth, nativeTextHeight,
			nativeLabel, labelMce, textSize, false, textAlpha);
		McrRender::drawText(roundf(nativeValuePos.x), roundf(nativeValuePos.y), nativeValueWidth, nativeTextHeight,
			nativeValue, valueMce, textSize, false, 1.f);
	}
	else {
		McrRender::drawText(roundf(nativeValuePos.x), roundf(nativeValuePos.y), nativeValueWidth, nativeTextHeight,
			nativeValue, valueMce, textSize, false, 1.f);
		McrRender::drawText(roundf(nativeSeparatorPos.x), roundf(nativeSeparatorPos.y), nativeSeparatorWidth,
			nativeTextHeight, " : ", labelMce, textSize, false, textAlpha);
		McrRender::drawText(roundf(nativeLabelPos.x), roundf(nativeLabelPos.y), nativeLabelWidth, nativeTextHeight,
			nativeLabelTrim, labelMce, textSize, false, textAlpha);
	}
}

void HudModule::drawEditorRect(const Vec4<float>& rect) {
	editorRect = rect;
	editorRectReady = true;
	hasLayoutRect = true;
	if (isEditing() && showOutlines) {
		const UIColor outline(255, 255, 255, isDragging ? 255 : 100);
		ImGuiUtils::drawRectangle(rect, outline, isDragging ? 2.f : 1.f, 2.f);
	}
}

bool HudModule::handleEditorInput(bool blocked) {
	if (!isEditing() || !show || !editorRectReady) {
		resetEditorInteraction();
		return false;
	}
	const Vec4<float>& rect = editorRect;
	static auto clampf = [](float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); };
	ImGuiIO& io = ImGui::GetIO();
	Vec2<float> screen(io.DisplaySize.x, io.DisplaySize.y);
	const float rectW = (std::max)(0.f, rect.z - rect.x);
	const float rectH = (std::max)(0.f, rect.w - rect.y);
	Vec2<float> mousePos(io.MousePos.x, io.MousePos.y);
	const bool leftDown = io.MouseDown[0];

	if (!blocked && !isDragging && g_dragOwner == nullptr && io.MouseClicked[0] && rect.contains(mousePos)) {
		position = Vec2<float>(anchoredRight ? rect.z : rect.x, rect.y);
		defaultBottomRow = -1;
		isDragging = true;
		g_dragOwner = this;
		mousePrev = mousePos;
		dragMouseStart = mousePos;
		dragPositionStart = position;
	}
	if (isDragging) {
		if (leftDown) {
			Vec2<float> newPos = dragPositionStart.add(mousePos.sub(dragMouseStart));
			if (anchoredRight)
				newPos.x = clampf(newPos.x, rectW, screen.x);
			else
				newPos.x = clampf(newPos.x, 0.f, (std::max)(0.f, screen.x - rectW));
			newPos.y = clampf(newPos.y, 0.f, (std::max)(0.f, screen.y - rectH));
			position = newPos;
		}
		else {
			Vec2<float> topLeft(anchoredRight ? position.x - rectW : position.x, position.y);
			const float startX = anchoredRight ? dragPositionStart.x - rectW : dragPositionStart.x;
			const Vec4<float> startRect(startX, dragPositionStart.y,
				startX + rectW, dragPositionStart.y + rectH);
			topLeft = arrangeDroppedHud(this, startRect, topLeft, screen);
			position = Vec2<float>(anchoredRight ? topLeft.x + rectW : topLeft.x, topLeft.y);
			isDragging = false;
			g_dragOwner = nullptr;
			HudEditor::saveHudConfig();
		}
	}
	mousePrev = mousePos;

	if (!blocked && !g_dragOwner && rect.contains(mousePos) && io.MouseClicked[1]) {
		if (ClickGui* gui = ModuleManager::getModule<ClickGui>()) gui->expandHudElement(this);
		return true;
	}
	return isDragging || (!blocked && rect.contains(mousePos));
}

std::string HudModule::serialize() const {
	char buf[256];
	snprintf(buf, sizeof(buf), "%s|%d|%.1f|%.1f|%d|%d,%d,%d,%d|%d|%d|%d|%d|%d",
		getModuleName().c_str(),
		(int)isEnabled(),
		position.x, position.y,
		(int)anchoredRight,
		labelColor.r, labelColor.g, labelColor.b, labelColor.a,
		(int)showOutlines,
		hudFontSize, opacity, spacing, defaultBottomRow);
	return std::string(buf);
}

void HudModule::deserialize(const std::string& line) {
	std::vector<std::string> parts;
	std::string cur;
	for (char c : line) {
		if (c == '|') { parts.push_back(cur); cur.clear(); }
		else cur += c;
	}
	parts.push_back(cur);

	auto toInt = [](const std::string& s) { return atoi(s.c_str()); };
	auto toFloat = [](const std::string& s) { return (float)atof(s.c_str()); };

	try {
		if (parts.size() < 10) return;
		hasLayoutRect = false;
		position.x = toFloat(parts[2]);
		position.y = toFloat(parts[3]);
		defaultBottomRow = parts.size() > 10 ? (std::clamp)(toInt(parts[10]), -1, 3) : -1;
		anchoredRight = toInt(parts[4]) != 0;

		std::string col = parts[5];
		int r = 255, g = 255, b = 255, a = 255;
		if (sscanf_s(col.c_str(), "%d,%d,%d,%d", &r, &g, &b, &a) >= 4)
			labelColor = UIColor((uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);

		showOutlines = toInt(parts[6]) != 0;
		hudFontSize = (std::clamp)(toInt(parts[7]), 12, 100);
		opacity = (std::clamp)(toInt(parts[8]), 0, 255);
		spacing = toInt(parts[9]);
	}
	catch (...) {}
}
