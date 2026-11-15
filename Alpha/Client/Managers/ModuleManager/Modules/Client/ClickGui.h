#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "../Module.h"
#include "../Settings/KeybindSetting.h"
#include "../Settings/SliderSetting.h"
#include "../Settings/TextSetting.h"
#include "../Settings/ColorSetting.h"
#include "Utils/Maths.h"
#include "Utils/ColorUtil.h"
#include "HudEntryInputGuard.h"

struct ImFont;

void getModuleListByCategory(Category category, std::vector<Module*>& modList);

struct ClickModuleState {
	bool isExtended = false;
	float expandAnim = 0.f;
};

struct ClickWindow {
	std::string windowName;
	Vec2<float> pos = Vec2<float>(0.f, 0.f);
	bool extended = true;
	Category category;
	std::vector<Module*> modList;
	std::unordered_map<std::string, ClickModuleState> moduleMap;

	float visibleHeight = 0.f;
	float headerRenderHeight = 0.f;
	float renderWidth = 0.f;

	ClickWindow(const Vec2<float>& Pos, const Category& c);
	ClickModuleState* getClickModule(const std::string& name);
};

struct ClickGuiHachimiAnim {
	float progress = 0.f;
	bool target = false;
	bool initialized = false;
};

struct ClickGuiHachimiColorAnim {
	UIColor from;
	UIColor to;
	float elapsed = 1.f;
	bool initialized = false;
};

class ClickGui : public Module {
public:
	ClickGui();
	~ClickGui();

	static void setCategoryIconFont(ImFont* font) { categoryIconFont = font; }
	static ImFont* getCategoryIconFont() { return categoryIconFont; }
	static const char* getCategoryIconGlyph(Category category);

	bool runOnBackground() override { return true; }

	std::vector<ClickWindow*> windowList;
	bool isLeftClickDown = false;
	bool isRightClickDown = false;
	bool isHoldingLeftClick = false;
	bool isHoldingRightClick = false;
	bool shouldToggleLeftClick = false;
	bool shouldToggleRightClick = false;
	void init();

	using WindowPositions = std::unordered_map<std::string, Vec2<float>>;
	using WindowExpandedStates = std::unordered_map<std::string, bool>;
	WindowPositions getWindowPositions() const;
	void setWindowPositions(const WindowPositions& positions);
	WindowExpandedStates getWindowExpandedStates() const;
	void setWindowExpandedStates(const WindowExpandedStates& states);

	bool hasInitialized = false;

	bool isShiftDown = false;

	Vec2<float> mousePos = Vec2<float>(0.f, 0.f);
	Vec2<float> mouseDelta = Vec2<float>(0.f, 0.f);

	SliderSettingBase* draggingSliderSettingPtr = nullptr;
	KeybindSetting* capturingKbSettingPtr = nullptr;
	TextSetting* capturingTextSettingPtr = nullptr;
	ColorSetting* draggingColorSettingPtr = nullptr;

	void onImGuiRender(ImDrawList* drawlist) override;
	void onKeyUpdate(int key, bool isDown) override;

	void onEnable() override;
	void onDisable() override;

	void refreshBinds();
	void beginHudEditor();
	void endHudEditor();
	void renderHudEditor(ImDrawList* drawlist);
	void expandHudElement(Module* element);
	bool hudEditorWantsMouse() const;
	float getGuiSize() const { return guiSize; }
	float getBgBlur() const;
	void appendTextInput(const std::string& text);

private:
	std::unique_ptr<ClickWindow> hudWindow;
	bool hudMouseConsumed = false;
	bool hudWaitForRelease = false;
	HudEntryInputGuard hudEntryGuard;
	Vec2<float> hudPreviousMouse{};
	void resetHudInput();

	void computeLayoutPositions(std::vector<std::pair<Category, Vec2<float>>>& out);

	void renderHachimiLayout(ImDrawList* drawlist, ClickWindow* hudPanel = nullptr);
	bool handleHachimiKey(int key);
	void resetHachimiInput();
	void resetHachimiLayout();
	std::unordered_map<const void*, ClickGuiHachimiAnim> hachimiPanelAnims;
	std::unordered_map<const void*, ClickGuiHachimiAnim> hachimiExpandAnims;
	std::unordered_map<const void*, ClickGuiHachimiAnim> hachimiDropdownAnims;
	std::unordered_map<const void*, ClickGuiHachimiAnim> hachimiHoverAnims;
	std::unordered_map<const void*, ClickGuiHachimiColorAnim> hachimiColorAnims;
	std::unordered_map<const void*, bool> hachimiDropdownOpen;
	double hachimiOpenStart = 0.0;
	ClickWindow* hachimiDragWindow = nullptr;
	SliderSettingBase* hachimiEditSlider = nullptr;
	std::string hachimiEditBuffer;
	ColorSetting* hachimiHexSetting = nullptr;
	std::string hachimiHexBuffer;
	int hachimiColorChannel = -1;
	std::string hachimiTooltipText;
	ClickGuiHachimiAnim hachimiTooltipAnim;
	int hachimiTheme = 0;
	int hachimiButtonHeight = 4;
	int hachimiScrollSpeed = 30;
	bool hachimiDescriptions = true;
	bool hachimiLowerCase = false;
	bool hachimiGear = false;
	bool hachimiGrayText = false;
	UIColor hachimiBackground = UIColor(37, 37, 37, 255);
	UIColor hachimiFrame = UIColor(26, 26, 26, 255);
	bool hachimiOutline = false;
	bool hachimiIcon = false;
	bool hachimiInterval = false;
	bool hachimiModuleButton = true;
	int hachimiFadeButton = 0;
	int hachimiFrameStyle = 0;
	UIColor hachimiTintStart = UIColor(0, 0, 0, 50);
	UIColor hachimiTintMid = UIColor(0, 0, 0, 100);
	UIColor hachimiTintEnd = UIColor(0, 0, 0, 255);
	float hachimiUnit = 0.f;
	float hachimiUnitFrom = 0.f;
	float hachimiUnitTo = 0.f;
	float hachimiUnitElapsed = 0.f;

	int decimals = 1;
	float guiSize = 0.89514559507369995f;
	float bgBlur = 6.7121610641479492f;
	bool blur = true;
	bool sound = false;
	float soundPitch = 0.45212221145629883f;
	bool tips = true;

	int scrollingDirection = 0;
	WindowPositions windowPositions;
	WindowExpandedStates windowExpandedStates;

	static inline ImFont* categoryIconFont = nullptr;
};
