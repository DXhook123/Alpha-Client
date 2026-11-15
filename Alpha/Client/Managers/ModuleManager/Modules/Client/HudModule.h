#pragma once
#include "../Module.h"
#include "Utils/Maths.h"
#include "Utils/ColorUtil.h"
#include <mutex>
#include <string>
#include <vector>

class HudModule : public Module {
protected:
	Vec2<float> position = Vec2<float>(0.f, 0.f);
	Vec2<float> defaultPos = Vec2<float>(0.f, 0.f);
	Vec2<float> mousePrev = Vec2<float>(0.f, 0.f);
	Vec2<float> dragMouseStart{};
	Vec2<float> dragPositionStart{};
	bool anchoredRight = false;
	int defaultBottomRow = -1;
	bool isDragging = false;
	bool _wasLeftPressed = false;

	bool   showOutlines = true;
	int    hudFontSize = 24;
	int    spacing = -1;
	bool   resetPos = false;
	UIColor getHudColor(const UIColor& custom, double offset = 0.0) const;
	void drawLabeledRect(const std::string& label, const std::string& value);
	float hudTextWidth(const std::string& text, float scale) const;
	void drawHudText(const Vec2<float>& pos, const std::string& text, const UIColor& color, float scale);
	Vec2<float> getClampedRenderPosition(float width, float height);

public:
	HudModule(std::string name, std::string description, Category category);
	~HudModule() override;
	void initializeSettings(float* scale = nullptr, bool color = true);
	void setDefaultBottomRow(int row) { defaultBottomRow = row; anchoredRight = true; }
	bool isEnabled() const override { return show; }
	void setEnabled(bool enabled) override { show = enabled; }
	void toggle() override { show = !show; }
	static bool isEditing();
	void resetEditorInteraction();
	bool handleEditorInput(bool blocked);
	Vec4<float> editorRect{};
	bool editorRectReady = false;
	bool hasLayoutRect = false;
	bool reflowFontLayout = false;
	Vec4<float> fontLayoutRect{};
	HudModule* fontLayoutReference = nullptr;
	int fontLayoutDirection = 0;
	float fontLayoutGap = 0.f;
	bool fontLayoutAlignEnd = false;

	UIColor labelColor = UIColor(132, 91, 245, 202);
	bool   show = true;
	int    opacity = 0;
	void renderNative();
	void invalidateNativeText();
	void renderNativeExtras();

	std::string nativeLabel;
	std::string nativeLabelTrim;
	std::string nativeValue;
	Vec4<float> nativeRect{};
	Vec2<float> nativeLabelPos{};
	Vec2<float> nativeValuePos{};
	Vec2<float> nativeSeparatorPos{};
	float nativeLabelWidth = 0.f;
	float nativeValueWidth = 0.f;
	float nativeSeparatorWidth = 0.f;
	float nativeTextHeight = 0.f;
	float nativeTextSize = 1.f;
	int nativeOpacity = 125;
	UIColor nativeLabelColor = UIColor(255, 255, 255, 255);
	bool nativeAnchoredRight = false;
	bool nativeTextReady = false;
	mutable std::mutex nativeTextMutex;
	struct NativeTextItem {
		Vec2<float> pos;
		std::string text;
		UIColor color;
		float scale;
		float height;
	};
	std::vector<NativeTextItem> nativeTextItems;

	void setShow(bool s) { show = s; }
	bool getShow() const { return show; }
	void drawEditorRect(const Vec4<float>& rect);

	std::string serialize() const;
	void deserialize(const std::string& line);
};
