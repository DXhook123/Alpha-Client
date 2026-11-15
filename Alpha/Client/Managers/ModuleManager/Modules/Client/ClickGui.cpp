#include <imfx.h>
#include "ClickGui.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <Windows.h>

#include "../../ModuleManager.h"
#include "../../../CommandManager/CommandManager.h"
#include "Renderer/ImGuiUtils.h"
#include "ColorModule.h"
#include "SettingsModule.h"
#include "HudModule.h"
#include "HudEditor.h"
#include "../Settings/EnumSetting.h"
#include "../Settings/BoolSetting.h"
#include "Utils/ImGuiInput.h"
#include "sdk/Game.h"
#include "../ModuleTranslations.hpp"
#include <imgui.h>

static float clampf(float v, float lo, float hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

void getModuleListByCategory(Category category, std::vector<Module*>& modList) {
	if (category == Category::Hud) {
		if (HudEditor* editor = ModuleManager::getModule<HudEditor>())
			for (HudModule* element : editor->getElements()) modList.push_back(element);
		return;
	}
	for (Module* mod : ModuleManager::moduleList) {
		if (mod->getCategory() == category)
			modList.push_back(mod);
	}
}

static char mapKeyToChar(int key, bool shift) {
	if (key >= 'A' && key <= 'Z') return shift ? (char)key : (char)(key + 32);
	if (key >= '0' && key <= '9') {
		static const char shifted[] = ")!@#$%^&*(";
		return shift ? shifted[key - '0'] : (char)key;
	}
	if (key == VK_SPACE) return ' ';
	if (key == VK_OEM_MINUS) return shift ? '_' : '-';
	if (key == VK_OEM_PLUS) return shift ? '+' : '=';
	if (key == VK_OEM_1) return shift ? ':' : ';';
	if (key == VK_OEM_2) return shift ? '?' : '/';
	if (key == VK_OEM_3) return shift ? '~' : '`';
	if (key == VK_OEM_4) return shift ? '{' : '[';
	if (key == VK_OEM_5) return shift ? '|' : '\\';
	if (key == VK_OEM_6) return shift ? '}' : ']';
	if (key == VK_OEM_7) return shift ? '"' : '\'';
	if (key == VK_OEM_COMMA) return shift ? '<' : ',';
	if (key == VK_OEM_PERIOD) return shift ? '>' : '.';
	return 0;
}

static std::vector<Category> withoutEmptyCategories(std::vector<Category> order) {
	std::erase_if(order, [](Category c) {
		std::vector<Module*> mods;
		getModuleListByCategory(c, mods);
		return mods.empty();
	});
	return order;
}

const char* ClickGui::getCategoryIconGlyph(Category category) {
	switch (category) {
	case Category::COMBAT: return "c";
	case Category::MOVEMENT: return "f";
	case Category::RENDER: return "d";
	case Category::PLAYER: return "e";
	case Category::MISC: return "a";
	case Category::CLIENT: return "g";
	default: return "B";
	}
}

ClickWindow::ClickWindow(const Vec2<float>& Pos, const Category& c) {
	this->pos = Pos;
	this->category = c;
	this->extended = true;
	if      (c == Category::COMBAT)   this->windowName = "Combat";
	else if (c == Category::MISC)     this->windowName = "Exploit";
	else if (c == Category::RENDER)   this->windowName = "Visual";
	else if (c == Category::MOVEMENT) this->windowName = "Movement";
	else if (c == Category::PLAYER)   this->windowName = "Player";
	else if (c == Category::CLIENT)   this->windowName = "Client";
	else if (c == Category::WORLD)    this->windowName = "World";
	else if (c == Category::Hud)      this->windowName = "Hud";
	else this->windowName = "Other";
	getModuleListByCategory(c, modList);
	std::sort(modList.begin(), modList.end(),
		[](Module* lhs, Module* rhs) { return lhs->getModuleName() < rhs->getModuleName(); });
}

ClickModuleState* ClickWindow::getClickModule(const std::string& name) {
	return &moduleMap[name];
}

ClickGui::ClickGui() : Module("ClickGui", "A GUI that displays every module", Category::CLIENT, VK_INSERT) {
	registerSetting(new SliderSetting<float>("Size", "GUI scale", &guiSize, 0.89514559507369995f, 0.5f, 2.f));
	registerSetting(new SliderSetting<int>("Decimals", "Slider decimal places", &decimals, 1, 1, 5));
	registerSetting(new EnumSetting("Theme", "Panel theme",
		{ "Astolfo", "Mio", "Custom" }, &hachimiTheme, 0));
	registerSetting(new SliderSetting<int>("Button Height", "The button height",
		&hachimiButtonHeight, 4, 0, 6));
	registerSetting(new SliderSetting<int>("Scroll Speed", "The speed of GUI scrolling",
		&hachimiScrollSpeed, 30, 5, 100));
	registerSetting(new BoolSetting("Descriptions", "Shows feature descriptions",
		&hachimiDescriptions, true));
	registerSetting(new BoolSetting("Lower Case", "All text to lowercase", &hachimiLowerCase, false));
	registerSetting(new BoolSetting("Gear", "Draw the gear", &hachimiGear, false));
	registerSetting(new BoolSetting("Gray Text", "Gray text for disabled module", &hachimiGrayText, false));

	const auto custom = [this]() { return hachimiTheme == 2; };
	registerSetting(new ColorSetting("Back Ground", "The background color",
		&hachimiBackground, UIColor(37, 37, 37, 255), custom));
	registerSetting(new ColorSetting("Frame", "The frame color",
		&hachimiFrame, UIColor(26, 26, 26, 255), custom));
	registerSetting(new BoolSetting("Outline", "Draw frame outline", &hachimiOutline, false, custom));
	registerSetting(new BoolSetting("Icon", "Draw module category icon", &hachimiIcon, false, custom));
	registerSetting(new BoolSetting("Interval", "Add interval", &hachimiInterval, false, custom));
	registerSetting(new BoolSetting("Module Button", "Draw module button", &hachimiModuleButton, true, custom));
	registerSetting(new EnumSetting("Fade Button", "Gradient",
		{ "Flat", "Horizontal", "Vertical" }, &hachimiFadeButton, 0,
		[this]() { return hachimiTheme == 2 && hachimiModuleButton; }));
	registerSetting(new EnumSetting("Frame Style", "Style",
		{ "Frame", "Slim" }, &hachimiFrameStyle, 0, custom));
	registerSetting(new ColorSetting("Tint Start", "Tint 1", &hachimiTintStart, UIColor(0, 0, 0, 50)));
	registerSetting(new ColorSetting("Tint Mid", "Tint 2", &hachimiTintMid, UIColor(0, 0, 0, 100)));
	registerSetting(new ColorSetting("Tint End", "Tint 3", &hachimiTintEnd, UIColor(0, 0, 0, 255)));

	registerSetting(new BoolSetting("Blur", "Blur the game background", &blur, true));
	registerSetting(new SliderSetting<float>("Bg Blur", "Backdrop blur strength", &bgBlur, 6.7121610641479492f, 0.f, 20.f,
		[this]() { return blur; }));
	registerSetting(new BoolSetting("Sound", "ClickGUI sound", &sound, false));
	registerSetting(new SliderSetting<float>("Sound Pitch", "ClickGUI sound pitch", &soundPitch, 0.45212221145629883f, 0.f, 2.f,
		[this]() { return sound; }));
	registerSetting(new BoolSetting("Tips", "Show usage tips", &tips, true));
}

ClickGui::~ClickGui() {
	for (ClickWindow* window : windowList) delete window;
}

ClickGui::WindowPositions ClickGui::getWindowPositions() const {
	WindowPositions positions = windowPositions;
	for (const ClickWindow* window : windowList)
		if (window) positions[window->windowName] = window->pos;
	return positions;
}

void ClickGui::setWindowPositions(const WindowPositions& positions) {
	windowPositions = positions;
	for (ClickWindow* window : windowList) {
		if (!window) continue;
		auto it = windowPositions.find(window->windowName);
		if (it != windowPositions.end()) window->pos = it->second;
	}
}

ClickGui::WindowExpandedStates ClickGui::getWindowExpandedStates() const {
	WindowExpandedStates states = windowExpandedStates;
	for (const ClickWindow* window : windowList)
		if (window) states[window->windowName] = window->extended;
	return states;
}

void ClickGui::setWindowExpandedStates(const WindowExpandedStates& states) {
	windowExpandedStates = states;
	for (ClickWindow* window : windowList) {
		if (!window) continue;
		auto it = windowExpandedStates.find(window->windowName);
		if (it != windowExpandedStates.end()) window->extended = it->second;
	}
}

void ClickGui::computeLayoutPositions(std::vector<std::pair<Category, Vec2<float>>>& out) {
	out.clear();
	const std::vector<Category> order = withoutEmptyCategories({
		Category::COMBAT, Category::MISC, Category::RENDER,
		Category::MOVEMENT, Category::PLAYER, Category::WORLD,
		Category::CLIENT
	});
	for (size_t i = 0; i < order.size(); ++i)
		out.emplace_back(order[i], Vec2<float>(15.f + (float)i * 108.f, 15.f));
}

void ClickGui::init() {
	for (auto* window : windowList) delete window;
	windowList.clear();
	std::vector<std::pair<Category, Vec2<float>>> positions;
	computeLayoutPositions(positions);
	for (auto& p : positions)
		windowList.emplace_back(new ClickWindow(p.second, p.first));
	for (auto* window : windowList) {
		if (auto it = windowPositions.find(window->windowName); it != windowPositions.end())
			window->pos = it->second;
		if (auto it = windowExpandedStates.find(window->windowName); it != windowExpandedStates.end())
			window->extended = it->second;
	}
	hasInitialized = true;
}

float ClickGui::getBgBlur() const {
	if (!blur) return 0.f;
	const float progress = ImGui::GetCurrentContext()
		? clampf((float)(ImGui::GetTime() - hachimiOpenStart) / 0.3f, 0.f, 1.f) : 1.f;
	return bgBlur * progress;
}

void ClickGui::refreshBinds() {
	for (auto* window : windowList) {
		window->modList.clear();
		getModuleListByCategory(window->category, window->modList);
		std::sort(window->modList.begin(), window->modList.end(),
			[](Module* lhs, Module* rhs) { return lhs->getModuleName() < rhs->getModuleName(); });
	}
}

void ClickGui::onEnable() {
	resetHachimiInput();
	hachimiOpenStart = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
	Game::setTextInputActive(false);
	if (auto ci = Game::getClientInstance())
		ci->releaseMouse();
	ImGuiInput::beginOverlayInput();
	isLeftClickDown = isRightClickDown = false;
	isHoldingLeftClick = isHoldingRightClick = false;
	shouldToggleLeftClick = shouldToggleRightClick = false;
	capturingKbSettingPtr = nullptr;
	capturingTextSettingPtr = nullptr;
	draggingSliderSettingPtr = nullptr;
	draggingColorSettingPtr = nullptr;
}

void ClickGui::onDisable() {
	resetHachimiInput();
	ImGuiInput::endOverlayInput();
	ImGuiInput::resetMouseState();
	isLeftClickDown = isRightClickDown = false;
	isHoldingLeftClick = isHoldingRightClick = false;
	shouldToggleLeftClick = shouldToggleRightClick = false;
	capturingKbSettingPtr = nullptr;
	if (capturingTextSettingPtr) capturingTextSettingPtr->editing = false;
	capturingTextSettingPtr = nullptr;
	draggingSliderSettingPtr = nullptr;
	draggingColorSettingPtr = nullptr;

	if (HudEditor* he = ModuleManager::getModule<HudEditor>())
		if (he->isEnabled()) he->setEnabled(false);

	Game::restoreMouseAfterOverlay();
}

void ClickGui::appendTextInput(const std::string& text) {
	if (!capturingTextSettingPtr || text.empty()) return;
	std::string& value = capturingTextSettingPtr->editBuffer;
	if (value.size() + text.size() > capturingTextSettingPtr->maxLength) return;
	value += text;
	*capturingTextSettingPtr->value = value;
	CommandManager::markForSave();
}

void ClickGui::onKeyUpdate(int key, bool isDown) {
	if (key == VK_SHIFT) { isShiftDown = isDown; return; }
	if (!isDown) return;
	if (isEnabled() && handleHachimiKey(key)) return;

	if (capturingTextSettingPtr) {
		std::string& value = capturingTextSettingPtr->editBuffer;
		if (key == VK_ESCAPE) {
			value = capturingTextSettingPtr->backupValue;
			*capturingTextSettingPtr->value = value;
			capturingTextSettingPtr->editing = false;
			capturingTextSettingPtr = nullptr;
			return;
		}
		if (key == VK_RETURN) {
			capturingTextSettingPtr->editing = false;
			*capturingTextSettingPtr->value = value;
			capturingTextSettingPtr = nullptr;
			CommandManager::markForSave();
			return;
		}
		if (key == VK_BACK) {
			if (!value.empty()) value.pop_back();
			*capturingTextSettingPtr->value = value;
			CommandManager::markForSave();
			return;
		}
		const char c = mapKeyToChar(key, isShiftDown);
		if (c != 0 && value.size() < capturingTextSettingPtr->maxLength) {
			value.push_back(c);
			*capturingTextSettingPtr->value = value;
			CommandManager::markForSave();
		}
		return;
	}

	if (capturingKbSettingPtr) {
		if (key == VK_ESCAPE || key == VK_DELETE)
			*capturingKbSettingPtr->value = 0;
		else
			*capturingKbSettingPtr->value = key;
		capturingKbSettingPtr = nullptr;
		refreshBinds();
		CommandManager::markForSave();
		return;
	}

	if (!isEnabled()) {
		if (key == getKeybind()) setEnabled(true);
		return;
	}
	if (key == getKeybind() || key == VK_ESCAPE)
		setEnabled(false);
}

void ClickGui::resetHudInput() {
	auto resetSettings = [](ClickWindow* window) {
		if (!window) return;
		for (Module* module : window->modList)
			for (Setting* setting : module->getSettingList())
				setting->isDragging = setting->isDragging2 = setting->isDragging3 = false;
	};
	for (ClickWindow* window : windowList) resetSettings(window);
	resetSettings(hudWindow.get());
	hudEntryGuard.reset();
	if (capturingTextSettingPtr) capturingTextSettingPtr->editing = false;
	capturingTextSettingPtr = nullptr;
	capturingKbSettingPtr = nullptr;
	draggingSliderSettingPtr = nullptr;
	draggingColorSettingPtr = nullptr;
	isLeftClickDown = isRightClickDown = false;
	isHoldingLeftClick = isHoldingRightClick = false;
	shouldToggleLeftClick = shouldToggleRightClick = false;
	isShiftDown = false;
	hudMouseConsumed = false;
	scrollingDirection = 0;
	resetHachimiInput();
}

void ClickGui::beginHudEditor() {
	resetHudInput();
	hudWaitForRelease = true;
	if (ImGui::GetCurrentContext()) {
		const ImGuiIO& io = ImGui::GetIO();
		const ImVec2 mouse = io.MousePos;
		hudPreviousMouse = Vec2<float>(mouse.x, mouse.y);
		if (io.MouseClicked[0] || io.MouseDown[0])
			hudEntryGuard.arm(ImGui::GetTime(), mouse.x, mouse.y,
				(std::max)(io.MouseDoubleClickTime, static_cast<float>(GetDoubleClickTime()) / 1000.f),
				io.MouseDoubleClickMaxDist);
	}
}

void ClickGui::endHudEditor() {
	resetHudInput();
	Module::setEnabled(false);
}

void ClickGui::expandHudElement(Module* element) {
	if (!hudWindow || !element) return;
	hudWindow->extended = true;
	for (Module* entry : hudWindow->modList) {
		ClickModuleState* state = hudWindow->getClickModule(entry->getModuleName());
		state->isExtended = entry == element;
		state->expandAnim = entry == element ? 1.f : 0.f;
	}
}

bool ClickGui::hudEditorWantsMouse() const {
	if (!hudWindow) return false;
	for (Module* module : hudWindow->modList)
		for (Setting* setting : module->getSettingList())
			if (setting->isDragging || setting->isDragging2 || setting->isDragging3) return true;
	const Vec4<float> bounds(hudWindow->pos.x, hudWindow->pos.y,
		hudWindow->pos.x + hudWindow->renderWidth,
		hudWindow->pos.y + hudWindow->headerRenderHeight + hudWindow->visibleHeight);
	return hudMouseConsumed || bounds.contains(mousePos) || draggingSliderSettingPtr || draggingColorSettingPtr
		|| (hachimiDragWindow && hachimiDragWindow == hudWindow.get());
}

void ClickGui::renderHudEditor(ImDrawList* drawlist) {
	setChineseMode(SettingsModule::getLanguage() == 1);
	ImGuiUtils::setDrawList(drawlist);
	ImGuiIO& io = ImGui::GetIO();
	mousePos = Vec2<float>(io.MousePos.x, io.MousePos.y);
	mouseDelta = mousePos.sub(hudPreviousMouse);
	hudPreviousMouse = mousePos;
	isHoldingLeftClick = isLeftClickDown = io.MouseDown[0];
	isHoldingRightClick = isRightClickDown = io.MouseDown[1];
	shouldToggleLeftClick = io.MouseClicked[0];
	shouldToggleRightClick = io.MouseClicked[1];
	scrollingDirection = io.MouseWheel > 0.f ? -1 : io.MouseWheel < 0.f ? 1 : 0;
	hudMouseConsumed = false;
	if (hudEntryGuard.blocks(ImGui::GetTime(), mousePos.x, mousePos.y,
		io.MouseClicked[0] || io.MouseClicked[1]))
		hudWaitForRelease = true;
	if (hudWaitForRelease) {
		if (!io.MouseDown[0] && !io.MouseDown[1]) hudWaitForRelease = false;
		shouldToggleLeftClick = shouldToggleRightClick = false;
		isHoldingLeftClick = isHoldingRightClick = isLeftClickDown = isRightClickDown = false;
	}
	if (!hudWindow)
		hudWindow = std::make_unique<ClickWindow>(Vec2<float>(24.f, 60.f), Category::Hud);
	HudEditor* editor = ModuleManager::getModule<HudEditor>();
	if (editor) hudWindow->pos = editor->categoryPosition;
	if (!isHoldingLeftClick)
		draggingColorSettingPtr = nullptr;
	if (capturingTextSettingPtr && shouldToggleRightClick) {
		*capturingTextSettingPtr->value = capturingTextSettingPtr->backupValue;
		capturingTextSettingPtr->editing = false;
		capturingTextSettingPtr = nullptr;
		shouldToggleRightClick = false;
		CommandManager::markForSave();
	}
	renderHachimiLayout(drawlist, hudWindow.get());
	if (editor && (editor->categoryPosition.x != hudWindow->pos.x || editor->categoryPosition.y != hudWindow->pos.y)) {
		editor->categoryPosition = hudWindow->pos;
		CommandManager::markForSave();
	}
	hudMouseConsumed = hudWaitForRelease || (io.MouseClicked[0] && !shouldToggleLeftClick) || (io.MouseClicked[1] && !shouldToggleRightClick);
	shouldToggleLeftClick = shouldToggleRightClick = false;
	scrollingDirection = 0;
}

void ClickGui::onImGuiRender(ImDrawList* drawlist) {
	setChineseMode(SettingsModule::getLanguage() == 1);
	if (!isEnabled()) return;
	if (HudModule::isEditing()) return;
	ImGuiUtils::setDrawList(drawlist);

	ImGuiIO& io = ImGui::GetIO();
	const Vec2<float> screen(io.DisplaySize.x, io.DisplaySize.y);

	mousePos = Vec2<float>(io.MousePos.x, io.MousePos.y);
	if (io.MouseClicked[0]) { isLeftClickDown = true; shouldToggleLeftClick = true; }
	isHoldingLeftClick = io.MouseDown[0];
	if (!io.MouseDown[0]) isLeftClickDown = false;
	if (io.MouseClicked[1]) { isRightClickDown = true; shouldToggleRightClick = true; }
	isHoldingRightClick = io.MouseDown[1];
	if (!io.MouseDown[1]) isRightClickDown = false;
	if (io.MouseWheel > 0) scrollingDirection = -1;
	else if (io.MouseWheel < 0) scrollingDirection = 1;

	static Vec2<float> oldMousePos = mousePos;
	mouseDelta = Vec2<float>(mousePos.x - oldMousePos.x, mousePos.y - oldMousePos.y);
	oldMousePos = mousePos;

	if (capturingKbSettingPtr && shouldToggleRightClick) {
		*capturingKbSettingPtr->value = 0;
		capturingKbSettingPtr = nullptr;
		shouldToggleRightClick = false;
		refreshBinds();
	}
	if (capturingTextSettingPtr && shouldToggleRightClick) {
		*capturingTextSettingPtr->value = capturingTextSettingPtr->backupValue;
		capturingTextSettingPtr->editBuffer = capturingTextSettingPtr->backupValue;
		capturingTextSettingPtr->editing = false;
		capturingTextSettingPtr = nullptr;
		shouldToggleRightClick = false;
	}

	if (!hasInitialized || windowList.empty())
		init();

	const float blurStrength = clampf(getBgBlur(), 0.f, 20.f);
	if (blurStrength > 0.01f && ImFX::Begin(drawlist)) {
		ImFX::AddBlur(blurStrength, ImVec4(0.f, 0.f, screen.x, screen.y), 0.f);
		ImFX::End(false);
	}

	renderHachimiLayout(drawlist);

	bool changed = false;
	for (const ClickWindow* window : windowList) {
		if (!window) continue;
		auto it = windowPositions.find(window->windowName);
		if (it == windowPositions.end() || it->second.x != window->pos.x || it->second.y != window->pos.y) {
			windowPositions[window->windowName] = window->pos;
			changed = true;
		}
		auto stateIt = windowExpandedStates.find(window->windowName);
		if (stateIt == windowExpandedStates.end() || stateIt->second != window->extended) {
			windowExpandedStates[window->windowName] = window->extended;
			changed = true;
		}
	}
	if (changed) CommandManager::markForSave();

	shouldToggleLeftClick = false;
	shouldToggleRightClick = false;
	scrollingDirection = 0;
	if (!isHoldingLeftClick) isLeftClickDown = false;
	if (!isHoldingRightClick) isRightClickDown = false;
}
