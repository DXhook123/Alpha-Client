#include "ClickGui.h"
#include "Client/Client.h"
#include "HudEditor.h"
#include "../../ModuleManager.h"
#include "../Settings/BoolSetting.h"
#include "../Settings/ColorSetting.h"
#include "../Settings/EnumSetting.h"
#include "../Settings/SliderSetting.h"
#include "sdk/Game.h"
#include "sdk/font/McrRender.h"
#include <cstdio>
#include "ColorModule.h"
#include "CustomFont.h"
#include <algorithm>
#include <cmath>
#include "Renderer/ImGuiUtils.h"
#include "Utils/FileUtil.h"
#include "Utils/ImGuiInput.h"
#include "../../../CommandManager/CommandManager.h"

namespace {
	bool isClickGuiOpen() {
		ClickGui* clickGui = ModuleManager::getModule<ClickGui>();
		return clickGui && clickGui->isEnabled();
	}

	bool shouldRenderHudOverlay() {
		return !isClickGuiOpen() || HudModule::isEditing();
	}
}

static const char* kHudConfigFile = "HudConfig.txt";

HudEditor::HudEditor() : Module("HudEditor", "Edit and rearrange HUD elements", Category::CLIENT) {
	registerSetting(new EnumSetting("HUD Font", "Font renderer used by HUD", { "Minecraft", "ImGuiUtils" }, &fontMode, 1));
	pos.initializeSettings();
	fps.initializeSettings();
	speed.initializeSettings();
	ping.initializeSettings();
	tps.initializeSettings();
	fps.setDefaultBottomRow(0);
	ping.setDefaultBottomRow(1);
	speed.setDefaultBottomRow(2);
	tps.setDefaultBottomRow(3);
	arrayList.initializeSettings(&arrayList.size);
	watermark.initializeSettings();
	welcome.initializeSettings();
}

void HudEditor::onEnable() {
	Game::setTextInputActive(false);
	if (ClickGui* gui = ModuleManager::getModule<ClickGui>()) gui->beginHudEditor();
	if (auto ci = Game::getClientInstance()) ci->releaseMouse();
	ImGuiInput::beginOverlayInput();
}

void HudEditor::onDisable() {
	for (HudModule* element : getElements()) element->resetEditorInteraction();
	if (ClickGui* gui = ModuleManager::getModule<ClickGui>()) gui->endHudEditor();
	ImGuiInput::endOverlayInput();
	ImGuiInput::resetMouseState();
	ClickGui* gui = ModuleManager::getModule<ClickGui>();
	if (!gui || !gui->isEnabled())
		Game::restoreMouseAfterOverlay();
	saveHudConfig();
	CommandManager::markForSave();
}

void HudEditor::onImGuiRender(ImDrawList* drawlist) {
	const bool minecraftFont = fontMode == 0;
	const bool renderHud = shouldRenderHudOverlay();
	useMinecraftFont.store(minecraftFont, std::memory_order_release);
	McrRender::setUseMinecraftFont(minecraftFont);
	nativePositionShown.store(renderHud && pos.getShow(), std::memory_order_release);
	nativeFpsShown.store(renderHud && fps.getShow(), std::memory_order_release);
	nativeSpeedShown.store(renderHud && speed.getShow(), std::memory_order_release);
	nativePingShown.store(renderHud && ping.getShow(), std::memory_order_release);
	nativeTpsShown.store(renderHud && tps.getShow(), std::memory_order_release);

	const auto elements = getElements();
	const auto* customFont = ModuleManager::getModule<CustomFont>();
	const int fontType = customFont ? customFont->fontType : -1;
	const int fontStyle = customFont ? customFont->fontStyle : 0;
	const float fontScale = ImGuiUtils::getFontScale();
	const bool fontChanged = fontLayoutInitialized &&
		(fontType != lastLayoutFontType || fontStyle != lastLayoutFontStyle
			|| fontMode != lastLayoutFontMode || fontScale != lastLayoutFontScale);
	const ImVec2 layoutScreen = ImGui::GetIO().DisplaySize;
	for (auto* element : elements) {
		element->reflowFontLayout = renderHud && fontChanged && element->getShow() && element->hasLayoutRect;
		element->fontLayoutRect = element->editorRect;
		element->fontLayoutReference = nullptr;
		element->fontLayoutDirection = 0;
	}
	for (auto* element : elements) {
		if (!element->reflowFontLayout) continue;
		const auto& rect = element->fontLayoutRect;
		float nearest = 9.f;
		for (auto* other : elements) {
			if (other == element || !other->reflowFontLayout) continue;
			const auto& candidate = other->fontLayoutRect;
			const bool alignRight = std::abs(rect.z - candidate.z) <= 6.f;
			const bool alignLeft = std::abs(rect.x - candidate.x) <= 6.f;
			const bool bottomHalf = rect.y > layoutScreen.y * 0.5f;
			if ((alignRight || alignLeft) && bottomHalf == (candidate.y > layoutScreen.y * 0.5f)) {
				const float gap = bottomHalf ? candidate.y - rect.w : rect.y - candidate.w;
				if (gap >= 0.f && gap <= 8.f && gap < nearest) {
					nearest = gap;
					element->fontLayoutReference = other;
					element->fontLayoutDirection = bottomHalf ? 1 : 2;
					element->fontLayoutGap = gap;
					element->fontLayoutAlignEnd = alignRight;
				}
			}
			const bool alignBottom = std::abs(rect.w - candidate.w) <= 6.f;
			const bool alignTop = std::abs(rect.y - candidate.y) <= 6.f;
			const bool rightHalf = rect.x > layoutScreen.x * 0.5f;
			if ((alignBottom || alignTop) && rightHalf == (candidate.x > layoutScreen.x * 0.5f)) {
				const float gap = rightHalf ? candidate.x - rect.z : rect.x - candidate.z;
				if (gap >= 0.f && gap <= 8.f && gap < nearest) {
					nearest = gap;
					element->fontLayoutReference = other;
					element->fontLayoutDirection = rightHalf ? 3 : 4;
					element->fontLayoutGap = gap;
					element->fontLayoutAlignEnd = alignBottom;
				}
			}
		}
	}
	if (renderHud) {
		fontLayoutInitialized = true;
		lastLayoutFontType = fontType;
		lastLayoutFontStyle = fontStyle;
		lastLayoutFontMode = fontMode;
		lastLayoutFontScale = fontScale;
	}
	for (HudModule* element : elements) {
		element->invalidateNativeText();
		element->editorRectReady = false;
		if (!isEnabled() || !element->getShow()) element->resetEditorInteraction();
	}
	if (!renderHud) return;
	if (isEnabled()) {
		const ImVec2 screen = ImGui::GetIO().DisplaySize;
		UIColor guide = ColorModule::getThemeColor();
		guide.a = 100;
		ImGuiUtils::drawDashedLine(Vec2<float>(screen.x * 0.5f, 0.f), Vec2<float>(screen.x * 0.5f, screen.y), guide, 1.f, 8.f, 6.f);
		ImGuiUtils::drawDashedLine(Vec2<float>(0.f, screen.y * 0.5f), Vec2<float>(screen.x, screen.y * 0.5f), guide, 1.f, 8.f, 6.f);
	}

	std::array<int, 14> rendered{};
	auto renderElement = [&](auto&& self, size_t index) -> void {
		if (rendered[index] != 0) return;
		rendered[index] = 1;
		auto* element = elements[index];
		if (element->fontLayoutReference) {
			const auto reference = std::find(elements.begin(), elements.end(), element->fontLayoutReference);
			if (reference != elements.end()) self(self, static_cast<size_t>(reference - elements.begin()));
		}
		if (element->getShow())
			element->onImGuiRender(drawlist);
		rendered[index] = 2;
	};
	for (size_t i = 0; i < elements.size(); ++i) renderElement(renderElement, i);
	if (isEnabled()) {
		ClickGui* gui = ModuleManager::getModule<ClickGui>();
		if (gui) gui->renderHudEditor(ImGui::GetForegroundDrawList());
		bool blocked = gui && gui->hudEditorWantsMouse();
		auto elements = getElements();
		for (auto it = elements.rbegin(); it != elements.rend(); ++it)
			blocked = (*it)->handleEditorInput(blocked) || blocked;
		ImGuiUtils::setDrawList(drawlist);
	}
}

void HudEditor::onNormalTick(LocalPlayer* localPlayer) {
	pos.onNormalTick(localPlayer);
	fps.onNormalTick(localPlayer);
	speed.onNormalTick(localPlayer);
	ping.onNormalTick(localPlayer);
	tps.onNormalTick(localPlayer);
}

void HudEditor::handleScreenView(void* screenView) {
	(void)screenView;
}

void HudEditor::renderNative(MinecraftUIRenderContext* context) {
	if (!shouldRenderHudOverlay()) return;
	const bool minecraftFont = useMinecraftFont.load(std::memory_order_acquire);
	McrRender::setUseMinecraftFont(minecraftFont);
	if (!minecraftFont || !McrRender::begin(context)) return;

	if (nativePositionShown.load(std::memory_order_acquire)) pos.renderNative();
	if (nativeFpsShown.load(std::memory_order_acquire))      fps.renderNative();
	if (nativeSpeedShown.load(std::memory_order_acquire))    speed.renderNative();
	if (nativePingShown.load(std::memory_order_acquire))     ping.renderNative();
	if (nativeTpsShown.load(std::memory_order_acquire)) tps.renderNative();
	if (arrayList.getShow()) arrayList.renderNativeExtras();
	if (watermark.getShow()) watermark.renderNativeExtras();
	if (welcome.getShow()) welcome.renderNativeExtras();
	if (potions.getShow()) potions.renderNativeExtras();

	McrRender::end();
}

std::string HudEditor::serializeAll() const {
	std::string out = pos.serialize() + "\n" + fps.serialize() + "\n" + speed.serialize() + "\n" + ping.serialize() + "\n" + arrayList.serialize() +
		"\n" + watermark.serialize() + "\n" + welcome.serialize() + "\n" + potions.serialize() + "\n" + tps.serialize();
	return out;
}

void HudEditor::loadFromLines(const std::vector<std::string>& lines) {
	for (const std::string& line : lines) {
		if (line.empty()) continue;
		std::string modName = line.substr(0, line.find('|'));
		if      (modName == "Position") pos.deserialize(line);
		else if (modName == "FPS")      fps.deserialize(line);
		else if (modName == "Speed")    speed.deserialize(line);
		else if (modName == "Ping")     ping.deserialize(line);
		else if (modName == "TPS")      tps.deserialize(line);
		else if (modName == "ArrayList") arrayList.deserialize(line);
		else if (modName == "Watermark") watermark.deserialize(line);
		else if (modName == "Welcome") welcome.deserialize(line);
		else if (modName == "PotionHUD") potions.deserialize(line);
	}
}

bool HudEditor::saveHudConfig() {
	std::string path = FileUtil::getClientPath() + kHudConfigFile;
	FileUtil::createPath(FileUtil::getClientPath());
	std::string data;
	if (HudEditor* hud = ModuleManager::getModule<HudEditor>())
		data = hud->serializeAll();
	const bool saved = FileUtil::writeFile(path, data);
	CommandManager::markForSave();
	return saved;
}

void HudEditor::loadHudConfig() {
	std::string path = FileUtil::getClientPath() + kHudConfigFile;
	std::string content = FileUtil::readFile(path);
	if (content.empty()) return;

	std::vector<std::string> lines;
	std::string cur;
	for (char c : content) {
		if (c == '\n') { if (!cur.empty()) lines.push_back(cur); cur.clear(); }
		else cur += c;
	}
	if (!cur.empty()) lines.push_back(cur);

	if (HudEditor* hud = ModuleManager::getModule<HudEditor>())
		hud->loadFromLines(lines);
}
