#include "WelcomeHUD.h"

#include "Renderer/ImGuiUtils.h"
#include "sdk/Game.h"
#include "sdk/Classes/LocalPlayer.h"
#include "sdk/font/McrRender.h"

#include <algorithm>

namespace {
	std::string replacePlayer(std::string value, const std::string& playerName) {
		const std::string token = "%player%";
		std::size_t pos = 0;
		while ((pos = value.find(token, pos)) != std::string::npos) {
			value.replace(pos, token.size(), playerName);
			pos += playerName.size();
		}
		return value;
	}
}

WelcomeHUD::WelcomeHUD() : HudModule("Welcome", "Show a welcome message with the local player name", Category::Hud) {
	registerSetting(new TextSetting("Text", "Use %player% for your name", &format, "Welcome back, %player%!", 96));
	registerSetting(new BoolSetting("Background", "Draw a background behind the welcome message", &background, false));
	registerSetting(new BoolSetting("Shadow", "Draw a soft shadow behind the welcome message", &shadow, true));
	registerSetting(new SliderSetting<float>("Shadow Strength", "Opacity of the welcome message shadow", &shadowStrength, 0.75f, 0.f, 1.f, [this] { return shadow; }));
	registerSetting(new BoolSetting("Glow", "Draw a colored glow around the welcome message", &glow, true));
	registerSetting(new SliderSetting<float>("Glow Strength", "Opacity of the welcome message glow", &glowStrength, 0.9f, 0.f, 1.f, [this] { return glow; }));
	registerSetting(new SliderSetting<float>("Glow Size", "Tight spread of the welcome message glow", &glowSize, 3.5f, 0.75f, 8.f, [this] { return glow; }));
	defaultPos = Vec2<float>(5.f, 31.f);
	position = defaultPos;
}

std::string WelcomeHUD::getDisplayText() const {
	LocalPlayer* localPlayer = Game::getLocalPlayer();
	const std::string playerName = localPlayer && !localPlayer->getName().empty() ? localPlayer->getName() : "Player";
	return replacePlayer(format, playerName);
}

void WelcomeHUD::onImGuiRender(ImDrawList* drawlist) {
	(void)drawlist;
    const UIColor textColor=getHudColor(labelColor);
	const std::string displayText = getDisplayText();
	if (displayText.empty()) return;

	const float textScale = 0.8f * static_cast<float>(hudFontSize) / 25.f;
	const float padding = 5.f;
	const float width = hudTextWidth(displayText, textScale);
	const float height = ImGuiUtils::getTextHeight(textScale);
	const float rectWidth = width + padding * 2.f;
	const float rectHeight = height + padding * 2.f;
	const Vec2<float> renderPosition = getClampedRenderPosition(rectWidth, rectHeight);
	const Vec4<float> rect(renderPosition.x, renderPosition.y,
		renderPosition.x + rectWidth, renderPosition.y + rectHeight);

	if (background)
		ImGuiUtils::fillRoundedRectangle(rect, UIColor(9, 10, 14, opacity), 3.f);
	const Vec2<float> textPos(rect.x + padding, rect.y + padding);
	if (glow && !McrRender::isNativeHudEnabled()) {
		ImGuiUtils::drawTextGlow(textPos, displayText, textColor, textScale, glowStrength, glowSize);
	}
	if (shadow && !McrRender::isNativeHudEnabled()) {
		const uint8_t shadowAlpha = static_cast<uint8_t>(std::clamp(shadowStrength * 230.f, 0.f, 255.f));
		ImGuiUtils::drawText(Vec2<float>(textPos.x + 1.25f, textPos.y + 1.25f), displayText,
			UIColor(static_cast<uint8_t>(textColor.r * .03f), static_cast<uint8_t>(textColor.g * .03f),
				static_cast<uint8_t>(textColor.b * .03f), shadowAlpha), textScale, false);
	}
	drawHudText(textPos, displayText, textColor, textScale);
	drawEditorRect(rect);
}
