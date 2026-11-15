#include "WatermarkHUD.h"

#include "ClickGui.h"
#include "ColorModule.h"
#include "Renderer/ImGuiUtils.h"
#include "Utils/ColorUtil.h"
#include "sdk/font/McrRender.h"

#include <algorithm>
#include <cmath>

namespace {
	std::string replaceAll(std::string value, const std::string& find, const std::string& replacement) {
		std::size_t pos = 0;
		while ((pos = value.find(find, pos)) != std::string::npos) {
			value.replace(pos, find.size(), replacement);
			pos += replacement.size();
		}
		return value;
	}

}

WatermarkHUD::WatermarkHUD() : HudModule("Watermark", "Show a configurable Alpha watermark", Category::Hud) {
	registerSetting(new TextSetting("Text", "Use %client% for Alpha", &text, "%client%", 64));
	registerSetting(new BoolSetting("Background", "Draw a background behind the watermark", &background, false));
	registerSetting(new BoolSetting("Pulse", "Pulse the watermark between theme and text colors", &pulse, true, [] { return !ColorModule::hasDynamicColors(); }));
	registerSetting(new BoolSetting("Shadow", "Draw a soft shadow behind the watermark", &shadow, true));
	registerSetting(new SliderSetting<float>("Shadow Strength", "Opacity of the watermark shadow", &shadowStrength, 0.75f, 0.f, 1.f, [this] { return shadow; }));
	registerSetting(new BoolSetting("Glow", "Draw a colored glow around the watermark", &glow, true));
	registerSetting(new SliderSetting<float>("Glow Strength", "Opacity of the watermark glow", &glowStrength, 0.9f, 0.f, 1.f, [this] { return glow; }));
	registerSetting(new SliderSetting<float>("Glow Size", "Tight spread of the watermark text glow", &glowSize, 3.5f, 0.75f, 8.f, [this] { return glow; }));
	defaultPos = Vec2<float>(5.f, 5.f);
	position = defaultPos;
}

std::string WatermarkHUD::getDisplayText() const {
	return replaceAll(text, "%client%", "Alpha");
}

void WatermarkHUD::onImGuiRender(ImDrawList* drawlist) {
	const std::string displayText = getDisplayText();
	if (displayText.empty()) return;

	const float textScale = 0.9f * static_cast<float>(hudFontSize) / 25.f;
	const float padding = 5.f;
	const float height = ImGuiUtils::getTextHeight(textScale);
	const float prefixW = 0.f;
	const float width = prefixW + hudTextWidth(displayText, textScale);
	const float rectWidth = width + padding * 2.f;
	const float rectHeight = height + padding * 2.f;
	const Vec2<float> renderPosition = getClampedRenderPosition(rectWidth, rectHeight);
	const Vec4<float> rect(renderPosition.x, renderPosition.y,
		renderPosition.x + rectWidth, renderPosition.y + rectHeight);
	const UIColor theme = ColorModule::getThemeColor();
	const UIColor textColor = ColorModule::hasDynamicColors() ? getHudColor(labelColor) :
        pulse ? ColorUtil::getWaveColor(theme, labelColor, 0) : labelColor;

	if (background)
		ImGuiUtils::fillRoundedRectangle(rect, UIColor(9, 10, 14, opacity), 3.f);
	if (glow && !McrRender::isNativeHudEnabled()) {
		ImGuiUtils::drawTextGlow(Vec2<float>(rect.x + padding + prefixW, rect.y + padding),
			displayText, textColor, textScale, glowStrength, glowSize);
	}
	if (shadow && !McrRender::isNativeHudEnabled()) {
		ImGuiUtils::drawText(Vec2<float>(rect.x + padding + prefixW + 1.25f, rect.y + padding + 1.25f),
			displayText, UIColor(static_cast<uint8_t>(textColor.r * .03f),
				static_cast<uint8_t>(textColor.g * .03f), static_cast<uint8_t>(textColor.b * .03f), 230),
			textScale, false);
	}
	drawHudText(Vec2<float>(rect.x + padding + prefixW, rect.y + padding), displayText, textColor, textScale);
	drawEditorRect(rect);
}
