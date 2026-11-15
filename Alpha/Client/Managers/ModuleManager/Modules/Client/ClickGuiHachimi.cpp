#include "ClickGui.h"
#include "ColorModule.h"
#include "../../ModuleManager.h"
#include "../../../CommandManager/CommandManager.h"
#include "../ModuleTranslations.hpp"
#include "Renderer/ImGuiUtils.h"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
constexpr float kPanelWidth = 105.f;
constexpr float kModuleWidth = 102.f;
constexpr float kSettingWidth = 100.f;
constexpr float kLine = 9.f;
constexpr float kTextPixelsPerUnit = 10.f;
constexpr float kPickerExtra = 29.5f + kSettingWidth;
const UIColor kGray(170, 170, 170, 255);
const UIColor kWhite(255, 255, 255, 255);
const UIColor kClear(0, 0, 0, 0);

float easeLinear(float x) { return x; }
float easeCubicOut(float x) { const float i = 1.f - x; return 1.f - i * i * i; }
float easeQuadOut(float x) { const float i = 1.f - x; return 1.f - i * i; }
float easeQuintOut(float x) { const float i = 1.f - x; return 1.f - i * i * i * i * i; }
float easeBackOut(float x) {
	constexpr float c1 = 1.70158f;
	constexpr float c3 = c1 + 1.f;
	const float t = x - 1.f;
	return 1.f + c3 * t * t * t + c1 * t * t;
}

float stepAnim(ClickGuiHachimiAnim& anim, bool target, float durationMs, float dt, float (*ease)(float)) {
	if (!anim.initialized) {
		anim.initialized = true;
		anim.progress = target ? 1.f : 0.f;
	}
	anim.target = target;
	const float step = durationMs <= 0.f ? 1.f : dt * 1000.f / durationMs;
	anim.progress = target ? (std::min)(1.f, anim.progress + step) : (std::max)(0.f, anim.progress - step);
	return ease(anim.progress);
}

UIColor lerpColor(const UIColor& a, const UIColor& b, float t) {
	auto mix = [t](uint8_t from, uint8_t to) {
		return static_cast<uint8_t>(static_cast<int>(from + (to - from) * t));
	};
	return UIColor(mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a));
}

UIColor stepColor(ClickGuiHachimiColorAnim& anim, const UIColor& target, float dt) {
	constexpr float duration = 0.15f;
	if (!anim.initialized) {
		anim.initialized = true;
		anim.from = target;
		anim.to = target;
		anim.elapsed = duration;
	}
	if (!(anim.to == target)) {
		anim.from = lerpColor(anim.from, anim.to, easeQuintOut((std::min)(1.f, anim.elapsed / duration)));
		anim.to = target;
		anim.elapsed = 0.f;
	}
	anim.elapsed += dt;
	return lerpColor(anim.from, anim.to, easeQuintOut((std::min)(1.f, anim.elapsed / duration)));
}

UIColor withAlpha(UIColor color, int alpha) {
	color.a = static_cast<uint8_t>(std::clamp(alpha, 0, 255));
	return color;
}

UIColor scaleRgb(UIColor color, float factor) {
	color.r = static_cast<uint8_t>(color.r * factor);
	color.g = static_cast<uint8_t>(color.g * factor);
	color.b = static_cast<uint8_t>(color.b * factor);
	return color;
}

UIColor hsbToRgb(float hue, float saturation, float brightness) {
	float r = brightness, g = brightness, b = brightness;
	if (saturation > 0.f) {
		const float h = (hue - std::floor(hue)) * 6.f;
		const float f = h - std::floor(h);
		const float p = brightness * (1.f - saturation);
		const float q = brightness * (1.f - saturation * f);
		const float t = brightness * (1.f - saturation * (1.f - f));
		switch (static_cast<int>(h)) {
		case 0: r = brightness; g = t; b = p; break;
		case 1: r = q; g = brightness; b = p; break;
		case 2: r = p; g = brightness; b = t; break;
		case 3: r = p; g = q; b = brightness; break;
		case 4: r = t; g = p; b = brightness; break;
		default: r = brightness; g = p; b = q; break;
		}
	}
	return UIColor(static_cast<uint8_t>(r * 255.f + 0.5f), static_cast<uint8_t>(g * 255.f + 0.5f),
		static_cast<uint8_t>(b * 255.f + 0.5f), 255);
}

std::string lowerAscii(std::string text) {
	for (char& c : text)
		if (static_cast<unsigned char>(c) < 0x80) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return text;
}

std::string keyName(int key) {
	if (!key) return "None";
	wchar_t wide[64]{};
	UINT scan = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);
	LONG flags = static_cast<LONG>(scan << 16);
	if ((key >= VK_PRIOR && key <= VK_DOWN) || key == VK_INSERT || key == VK_DELETE
		|| key == VK_RCONTROL || key == VK_RMENU || key == VK_DIVIDE || key == VK_NUMLOCK)
		flags |= 1 << 24;
	if (const int length = GetKeyNameTextW(flags, wide, 64)) {
		const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide, length, nullptr, 0, nullptr, nullptr);
		if (bytes > 0) {
			std::string utf8(bytes, '\0');
			WideCharToMultiByte(CP_UTF8, 0, wide, length, utf8.data(), bytes, nullptr, nullptr);
			return utf8;
		}
	}
	char text[32]{};
	std::snprintf(text, sizeof(text), "VK_%02X", key);
	return text;
}

std::string formatNumber(SliderSettingBase* slider, int decimals) {
	char buffer[64]{};
	if (slider->valueType == ValueType::INT_T) {
		std::snprintf(buffer, sizeof(buffer), "%d", *static_cast<SliderSetting<int>*>(slider)->valuePtr);
		return buffer;
	}
	std::snprintf(buffer, sizeof(buffer), "%.*f", std::clamp(decimals, 1, 5),
		*static_cast<SliderSetting<float>*>(slider)->valuePtr);
	return buffer;
}

std::string colorHex(const UIColor& color) {
	const unsigned int argb = (static_cast<unsigned int>(color.a) << 24) | (static_cast<unsigned int>(color.r) << 16)
		| (static_cast<unsigned int>(color.g) << 8) | static_cast<unsigned int>(color.b);
	char buffer[16]{};
	std::snprintf(buffer, sizeof(buffer), "%x", argb);
	return buffer;
}

bool parseHex(const std::string& text, UIColor& out) {
	if (text.empty() || text.size() > 8) return false;
	char* end = nullptr;
	const unsigned long value = std::strtoul(text.c_str(), &end, 16);
	if (!end || *end != '\0') return false;
	const bool hasAlpha = text.size() > 6;
	out = UIColor(static_cast<uint8_t>((value >> 16) & 0xFF), static_cast<uint8_t>((value >> 8) & 0xFF),
		static_cast<uint8_t>(value & 0xFF), hasAlpha ? static_cast<uint8_t>((value >> 24) & 0xFF) : 255);
	return true;
}

char digitFromKey(int key, bool shift) {
	if (!shift && key >= '0' && key <= '9') return static_cast<char>(key);
	if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) return static_cast<char>('0' + key - VK_NUMPAD0);
	if (key == VK_OEM_PERIOD || key == VK_DECIMAL) return '.';
	if (key == VK_OEM_MINUS || key == VK_SUBTRACT) return '-';
	return 0;
}

char hexFromKey(int key, bool shift) {
	if (!shift && key >= '0' && key <= '9') return static_cast<char>(key);
	if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) return static_cast<char>('0' + key - VK_NUMPAD0);
	if (key >= 'A' && key <= 'F') return static_cast<char>(shift ? key : key + 32);
	return 0;
}

bool inside(float px, float py, float x, float y, float w, float h) {
	return px >= x && px <= x + w && py >= y && py < y + h;
}

void playClick(bool enabled, float pitch) {
	if (!enabled) return;
	::Beep(static_cast<DWORD>(440.f * std::clamp(pitch, 0.25f, 2.f)), 28);
}

std::string caret() {
	return (GetTickCount64() / 250) % 2 == 0 ? "_" : "";
}

struct Painter {
	ImDrawList* list = nullptr;
	float unit = 2.f;
	float offsetY = 0.f;
	float alpha = 1.f;
	float textScale = 1.f;
	float lineHeight = kLine;

	ImVec2 at(float x, float y) const { return ImVec2(x * unit, y * unit + offsetY); }
	ImU32 col(UIColor c) const {
		c.a = static_cast<uint8_t>(c.a * std::clamp(alpha, 0.f, 1.f));
		return c.toImColor();
	}
	void fill(float x, float y, float w, float h, UIColor c) const {
		if (w < 0.f) { x += w; w = -w; }
		if (h < 0.f) { y += h; h = -h; }
		if (w <= 0.f || h <= 0.f || c.a == 0) return;
		list->AddRectFilled(at(x, y), at(x + w, y + h), col(c));
	}
	void horizontal(float x, float y, float w, float h, UIColor left, UIColor right) const {
		if (w <= 0.f || h <= 0.f) return;
		list->AddRectFilledMultiColor(at(x, y), at(x + w, y + h), col(left), col(right), col(right), col(left));
	}
	void vertical(float x, float y, float w, float h, UIColor top, UIColor bottom) const {
		if (w <= 0.f || h <= 0.f) return;
		list->AddRectFilledMultiColor(at(x, y), at(x + w, y + h), col(top), col(top), col(bottom), col(bottom));
	}
	float width(const std::string& text) const {
		return ImGuiUtils::getTextWidth(text, textScale) / unit;
	}
	void text(const std::string& value, float x, float y, UIColor c) const {
		if (value.empty()) return;
		c.a = static_cast<uint8_t>(c.a * std::clamp(alpha, 0.f, 1.f));
		const float height = ImGuiUtils::getTextHeight(textScale);
		const ImVec2 pos = at(x, y + lineHeight * 0.5f);
		ImGuiUtils::drawText(Vec2<float>(pos.x, pos.y - height * 0.5f), value, c, textScale, false);
	}
	std::string fit(const std::string& value, float maxW) const {
		if (maxW <= 0.f) return "";
		if (width(value) <= maxW) return value;
		const std::string ell = "...";
		std::string out = value;
		while (!out.empty() && width(out + ell) > maxW) {
			while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80) out.pop_back();
			if (!out.empty()) out.pop_back();
		}
		while (!out.empty() && out.back() == ' ') out.pop_back();
		if (out.empty()) return width(ell) <= maxW ? ell : "";
		return out + ell;
	}
	void labelValue(const std::string& label, float lx, const std::string& value, float rx, float y,
		UIColor labelColor, UIColor valueColor) const {
		constexpr float kGap = 3.f;
		const float total = rx - lx;
		const float minLabel = (std::min)(width(label), total * 0.4f);
		const std::string shownValue = fit(value, total - kGap - minLabel);
		const float valueW = width(shownValue);
		text(fit(label, total - kGap - valueW), lx, y, labelColor);
		text(shownValue, rx - valueW, y, valueColor);
	}
	void textShrink(const std::string& value, float x, float y, float maxW, UIColor c) {
		const float w = width(value);
		if (w <= maxW || w <= 0.f) { text(value, x, y, c); return; }
		const float saved = textScale;
		textScale = saved * (std::max)(0.7f, maxW / w);
		text(fit(value, maxW), x, y, c);
		textScale = saved;
	}
	void pushClip(float x1, float y1, float x2, float y2) const {
		list->PushClipRect(at(x1, y1), at(x2, y2), true);
	}
	void popClip() const { list->PopClipRect(); }
};
}

namespace {
struct HachimiTheme {
	bool outline;
	bool icon;
	bool moduleButton;
	int fade;
	bool interval;
	UIColor background;
	UIColor frame;
	bool slim;
};
}

void ClickGui::resetHachimiInput() {
	hachimiDragWindow = nullptr;
	hachimiEditSlider = nullptr;
	hachimiEditBuffer.clear();
	hachimiHexSetting = nullptr;
	hachimiHexBuffer.clear();
	hachimiColorChannel = -1;
	hachimiTooltipText.clear();
	hachimiTooltipAnim = {};
}

void ClickGui::resetHachimiLayout() {
	std::vector<std::pair<Category, Vec2<float>>> positions;
	computeLayoutPositions(positions);
	for (ClickWindow* window : windowList) {
		if (!window) continue;
		for (const auto& entry : positions) {
			if (entry.first != window->category) continue;
			window->pos = entry.second;
			windowPositions[window->windowName] = entry.second;
		}
		window->extended = true;
		for (auto& kv : window->moduleMap) kv.second.isExtended = false;
	}
	hachimiPanelAnims.clear();
	hachimiExpandAnims.clear();
	hachimiDropdownAnims.clear();
	hachimiDropdownOpen.clear();
	resetHachimiInput();
	CommandManager::markForSave();
}

bool ClickGui::handleHachimiKey(int key) {
	if (isShiftDown && key == VK_F1) {
		resetHachimiLayout();
		return true;
	}
	if (hachimiEditSlider) {
		if (key == VK_ESCAPE) {
			hachimiEditSlider = nullptr;
			hachimiEditBuffer.clear();
		} else if (key == VK_RETURN) {
			SliderSettingBase* slider = hachimiEditSlider;
			char* end = nullptr;
			const double value = std::strtod(hachimiEditBuffer.c_str(), &end);
			if (!hachimiEditBuffer.empty() && end && *end == '\0') {
				if (slider->valueType == ValueType::INT_T) {
					auto* s = static_cast<SliderSetting<int>*>(slider);
					*s->valuePtr = std::clamp(static_cast<int>(value), s->minValue, s->maxValue);
				} else {
					auto* s = static_cast<SliderSetting<float>*>(slider);
					*s->valuePtr = std::clamp(static_cast<float>(value), s->minValue, s->maxValue);
				}
				CommandManager::markForSave();
			}
			hachimiEditSlider = nullptr;
			hachimiEditBuffer.clear();
		} else if (key == VK_BACK) {
			if (!hachimiEditBuffer.empty()) hachimiEditBuffer.pop_back();
		} else if (const char c = digitFromKey(key, isShiftDown)) {
			if (hachimiEditBuffer.size() < 24) hachimiEditBuffer.push_back(c);
		}
		return true;
	}
	if (hachimiHexSetting) {
		if (key == VK_ESCAPE) {
			hachimiHexSetting = nullptr;
			hachimiHexBuffer.clear();
		} else if (key == VK_RETURN) {
			UIColor parsed;
			if (parseHex(hachimiHexBuffer, parsed)) {
				*hachimiHexSetting->value = parsed;
				hachimiHexSetting->syncHSVFromColor();
				CommandManager::markForSave();
			}
			hachimiHexSetting = nullptr;
			hachimiHexBuffer.clear();
		} else if (key == VK_BACK) {
			if (!hachimiHexBuffer.empty()) hachimiHexBuffer.pop_back();
		} else if (const char c = hexFromKey(key, isShiftDown)) {
			if (hachimiHexBuffer.size() < 8) hachimiHexBuffer.push_back(c);
		}
		return true;
	}
	return false;
}

void ClickGui::renderHachimiLayout(ImDrawList* drawlist, ClickWindow* hudPanel) {
	ImGuiIO& io = ImGui::GetIO();
	const float dt = std::clamp(io.DeltaTime, 0.f, 0.1f);
	const Vec2<float> screen(io.DisplaySize.x, io.DisplaySize.y);
	const bool hudMode = hudPanel != nullptr;

	const float openLinear = std::clamp(static_cast<float>(ImGui::GetTime() - hachimiOpenStart) / 0.3f, 0.f, 1.f);
	const float open = hudMode ? 1.f : easeBackOut(openLinear);

	float targetUnit = 2.f * std::clamp(guiSize, 0.25f, 4.f);
	const bool draggingSize = draggingSliderSettingPtr && draggingSliderSettingPtr->valueType == ValueType::FLOAT_T
		&& static_cast<SliderSetting<float>*>(draggingSliderSettingPtr)->valuePtr == &guiSize;
	if (hachimiUnit <= 0.f) {
		hachimiUnit = hachimiUnitFrom = hachimiUnitTo = targetUnit;
		hachimiUnitElapsed = 1.f;
	}
	if (draggingSize) targetUnit = hachimiUnitTo;
	if (targetUnit != hachimiUnitTo) {
		hachimiUnitFrom = hachimiUnit;
		hachimiUnitTo = targetUnit;
		hachimiUnitElapsed = 0.f;
	}
	hachimiUnitElapsed += dt;
	hachimiUnit = hachimiUnitFrom + (hachimiUnitTo - hachimiUnitFrom)
		* easeCubicOut((std::min)(1.f, hachimiUnitElapsed / 0.2f));

	Painter p;
	p.list = drawlist;
	p.unit = hachimiUnit;
	p.alpha = std::clamp(open, 0.f, 1.f);
	const float basePixels = ImGuiUtils::getFontSize();
	p.textScale = basePixels > 0.f ? kTextPixelsPerUnit * p.unit * (1.3f / 1.16f) / basePixels : 1.f;
	p.lineHeight = (std::max)(kLine, std::ceil(ImGuiUtils::getTextHeight(p.textScale) / p.unit));

	const auto display = [this](const std::string& text) {
		return hachimiLowerCase ? lowerAscii(text) : text;
	};

	const auto themeColor = [&](double y) -> UIColor {
		if (ColorModule* cm = ColorModule::instance())
			return withAlpha(cm->getColor(y * p.unit / 10.0), 255);
		return UIColor(181, 156, 255, 255);
	};

	HachimiTheme theme{};
	switch (hachimiTheme) {
	case 1:
		theme = { false, false, false, 0, false,
			withAlpha(themeColor(0.0), 60), withAlpha(themeColor(0.0), 180), false };
		break;
	case 2:
		theme = { hachimiOutline, hachimiIcon, hachimiModuleButton, hachimiFadeButton, hachimiInterval,
			hachimiBackground, hachimiFrame, hachimiFrameStyle == 1 };
		break;
	default:
		theme = { true, true, true, 0, false, UIColor(37, 37, 37, 255), UIColor(26, 26, 26, 255), false };
		break;
	}

	const float buttonPad = static_cast<float>(std::clamp(hachimiButtonHeight, 0, 6)) + 1.f;
	const float textPad = 1.5f + std::clamp(hachimiButtonHeight, 0, 6) / 2.f;
	const float rowH = p.lineHeight + buttonPad;
	const float headerH = p.lineHeight + 6.f;
	const float interval = theme.interval ? 0.5f : 0.f;
	const float slimInset = theme.slim ? 1.5f : 0.f;
	const float frameInset = !theme.slim && (theme.interval || !theme.moduleButton) ? 0.5f : 0.f;

	const float mx = mousePos.x / p.unit;
	const float my = mousePos.y / p.unit;
	const bool leftClick = shouldToggleLeftClick;
	const bool rightClick = shouldToggleRightClick;
	const bool middleClick = io.MouseClicked[2];
	const bool anyClick = leftClick || rightClick || middleClick;
	const bool leftHeld = isHoldingLeftClick;
	bool editHit = false;
	std::string tooltip;
	const auto click = [&]() { playClick(sound, soundPitch); };

	const auto stateColor = [&](const void* key, bool active, bool hovered, float y) {
		UIColor target = kClear;
		if (active) target = withAlpha(themeColor(y), hovered ? 200 : 160);
		else if (hovered) target = withAlpha(themeColor(y), 60);
		return stepColor(hachimiColorAnims[key], target, dt);
	};
	const auto fadeFill = [&](float x, float y, float w, float h, UIColor c) {
		if (theme.fade == 1) p.horizontal(x, y, w, h, c, scaleRgb(c, 0.76f));
		else if (theme.fade == 2) p.vertical(x, y, w, h, c, scaleRgb(c, 0.87f));
		else p.fill(x, y, w, h, c);
	};

	if (!leftHeld) {
		hachimiDragWindow = nullptr;
		draggingSliderSettingPtr = nullptr;
		draggingColorSettingPtr = nullptr;
		hachimiColorChannel = -1;
	}

	if (hudMode) hudPanel->pos = Vec2<float>(hudPanel->pos.x / p.unit, hudPanel->pos.y / p.unit);

	if (!hudMode && scrollingDirection != 0) {
		const float amount = -static_cast<float>(scrollingDirection) * static_cast<float>(hachimiScrollSpeed);
		for (ClickWindow* window : windowList) {
			if (!window) continue;
			if (isShiftDown) window->pos.x += amount;
			else window->pos.y += amount;
		}
	}
	if (hachimiDragWindow) {
		hachimiDragWindow->pos.x += mouseDelta.x / p.unit;
		hachimiDragWindow->pos.y += mouseDelta.y / p.unit;
	}
	if (!hudMode) {
		const ImU32 a = p.col(hachimiTintStart), b = p.col(hachimiTintMid), c = p.col(hachimiTintEnd);
		drawlist->AddRectFilledMultiColor(ImVec2(0.f, 0.f), ImVec2(screen.x, screen.y * 0.5f), a, a, b, b);
		drawlist->AddRectFilledMultiColor(ImVec2(0.f, screen.y * 0.5f), ImVec2(screen.x, screen.y), b, b, c, c);
	}

	if (tips && !hudMode) {
		static const char* tipsEn[] = {
			"Right-click a header to fold it, drag it to move",
			"Mouse wheel moves every panel, hold Shift for sideways",
			"Shift-click a module to toggle its ArrayList entry",
			"Shift-click a key bind to flip Hold / Toggle, Shift+F1 restores layout",
		};
		static const char* tipsZh[] = {
			"右键标题栏折叠面板，按住标题栏拖动",
			"滚轮整体移动面板，按住 Shift 改为横向",
			"Shift + 点击模块：在模块列表中显示/隐藏",
			"Shift + 点击按键绑定：切换按住/切换；Shift + F1 恢复布局",
		};
		for (int i = 0; i < 4; ++i)
			p.text(getChineseMode() ? tipsZh[i] : tipsEn[i], 1.f, 1.f + p.lineHeight * i, kWhite);
	}

	p.offsetY = screen.y / 8.f * (open - 1.f);

	const auto settingExtra = [&](Setting* setting) -> float {
		if (setting->type == SettingType::ENUM_S) {
			auto* es = static_cast<EnumSetting*>(setting);
			const float t = stepAnim(hachimiDropdownAnims[es], hachimiDropdownOpen[es], 200.f, 0.f, easeCubicOut);
			return (static_cast<float>(es->enumList.size()) * (p.lineHeight + 1.f) + 3.f) * t;
		}
		if (setting->type == SettingType::COLOR_S) {
			const float t = stepAnim(hachimiDropdownAnims[setting], hachimiDropdownOpen[setting], 200.f, 0.f, easeCubicOut);
			return kPickerExtra * t;
		}
		return 0.f;
	};
	const auto isVisible = [](Setting* setting) {
		return !setting->dependOn.has_value() || (*setting->dependOn)();
	};

	const auto colorRow = [&](ColorSetting* cs, const std::string& label, bool themeSetting, float sx, float sy,
		bool sHovered, bool rowVisible, bool interactive) -> float {
		bool& expanded = hachimiDropdownOpen[cs];
		const float pickerTop = sy + rowH;
		const float hexY = pickerTop + 17.5f + kSettingWidth;
		const bool hexEditing = hachimiHexSetting == cs;
		const bool hexHovered = rowVisible && inside(mx, my, sx + 0.5f, hexY, kSettingWidth - 15.5f, 12.f);
		if (hexEditing && hexHovered) editHit = true;
		if (sHovered && interactive && rightClick) {
			expanded = !expanded;
			click();
		}
		if (expanded && interactive && leftClick && rowVisible) {
			if (inside(mx, my, sx + 0.5f, pickerTop + 0.5f, kSettingWidth - 1.f, kSettingWidth - 1.f)) {
				draggingColorSettingPtr = cs;
				hachimiColorChannel = 0;
			} else if (inside(mx, my, sx + 0.5f, pickerTop + 0.5f + kSettingWidth, kSettingWidth - 1.f, 8.f)) {
				draggingColorSettingPtr = cs;
				hachimiColorChannel = 1;
			} else if (inside(mx, my, sx + 0.5f, pickerTop + 9.f + kSettingWidth, kSettingWidth - 1.f, 8.f)) {
				draggingColorSettingPtr = cs;
				hachimiColorChannel = 2;
			}
			if (hexHovered) {
				if (hexEditing) {
					handleHachimiKey(VK_RETURN);
				} else {
					hachimiHexSetting = cs;
					hachimiHexBuffer = colorHex(*cs->value);
					editHit = true;
				}
				click();
			}
			if (inside(mx, my, sx + kSettingWidth - 14.5f, hexY, 13.5f, 12.f)) {
				const UIColor themed = themeColor(sy);
				*cs->value = UIColor(themed.r, themed.g, themed.b, cs->value->a);
				cs->syncHSVFromColor();
				CommandManager::markForSave();
				click();
			}
		}
		if (draggingColorSettingPtr == cs && leftHeld) {
			const float clampT = 0.001f;
			if (hachimiColorChannel == 0) {
				cs->saturation = std::clamp((mx - (sx + 1.f)) / (kSettingWidth - 2.f), clampT, 1.f - clampT);
				cs->brightness = 1.f - std::clamp((my - (pickerTop + 0.5f)) / (kSettingWidth - 0.5f), clampT, 1.f - clampT);
			} else if (hachimiColorChannel == 1) {
				cs->hue = std::clamp((mx - (sx + 1.f)) / (kSettingWidth - 1.f), clampT, 1.f - clampT);
			} else if (hachimiColorChannel == 2) {
				cs->alpha = std::clamp((mx - (sx + 1.f)) / (kSettingWidth - 1.f), clampT, 1.f - clampT);
			}
			cs->syncColorFromHSV();
			CommandManager::markForSave();
		}

		fadeFill(sx, sy, kSettingWidth, rowH, stateColor(cs, false, sHovered, sy));
		p.fill(sx + kSettingWidth - 11.f, sy + (rowH - 10.f) / 2.f, 10.f, 10.f, *cs->value);
		p.text(p.fit(label, kSettingWidth - 16.f), sx + 2.f, sy + textPad, kWhite);

		const float pickT = stepAnim(hachimiDropdownAnims[cs], expanded, 200.f, dt, easeCubicOut);
		if (pickT <= 0.01f) return 0.f;
		p.pushClip(sx, pickerTop, sx + kSettingWidth, pickerTop + kPickerExtra * pickT);
		const UIColor hueColor = hsbToRgb(cs->hue, 1.f, 1.f);
		for (float i = 0.f; i < kSettingWidth - 1.f; i += 1.f)
			p.fill(sx + 0.5f + i, pickerTop + 0.5f + kSettingWidth, 1.f, 8.f,
				hsbToRgb(i / (kSettingWidth - 1.f), 1.f, 1.f));
		p.fill(sx + (kSettingWidth - 1.f) * cs->hue, pickerTop + 0.5f + kSettingWidth, 1.f, 8.f, kWhite);
		p.horizontal(sx + 0.5f, pickerTop + 0.5f, kSettingWidth - 1.f, kSettingWidth - 1.f, kWhite, hueColor);
		p.vertical(sx + 0.5f, pickerTop + 0.5f, kSettingWidth - 1.f, kSettingWidth - 1.f,
			UIColor(0, 0, 0, 0), UIColor(0, 0, 0, 255));
		p.fill(sx + kSettingWidth * cs->saturation - 1.f,
			pickerTop - 0.5f + kSettingWidth * (1.f - cs->brightness), 2.f, 2.f, kWhite);
		const float alphaY = pickerTop + 9.f + kSettingWidth;
		bool dark = true;
		for (float i = 0.f; i < kSettingWidth - 1.f; i += 4.f) {
			const float cw = (std::min)(4.f, kSettingWidth - 1.f - i);
			p.fill(sx + 0.5f + i, alphaY, cw, 4.f, dark ? UIColor(144, 144, 144, 255) : kWhite);
			p.fill(sx + 0.5f + i, alphaY + 4.f, cw, 4.f, dark ? kWhite : UIColor(144, 144, 144, 255));
			dark = !dark;
		}
		p.horizontal(sx + 0.5f, alphaY, kSettingWidth - 1.f, 8.f, UIColor(0, 0, 0, 0), hueColor);
		p.fill(sx + (kSettingWidth - 1.f) * cs->alpha, alphaY, 1.f, 8.f, kWhite);
		if (themeSetting)
			p.fill(sx + 1.f + kSettingWidth - 15.f, hexY, 13.5f, 12.f, withAlpha(themeColor(sy), 100));
		p.fill(sx + 0.5f, hexY, kSettingWidth - 15.f, 12.f, UIColor(144, 144, 144, 48));
		const std::string hex = "#" + (hexEditing ? hachimiHexBuffer + caret() : colorHex(*cs->value));
		p.text(hex, sx + 1.f, hexY + 1.5f, kWhite);
		{
			const ImVec2 center = p.at(sx + kSettingWidth - 7.75f, hexY + 6.f);
			const float r = 3.2f * p.unit;
			const ImU32 iconColor = p.col(kWhite);
			drawlist->PathArcTo(center, r, 0.35f, 2.75f, 12);
			drawlist->PathStroke(iconColor, 0, 0.6f * p.unit);
			drawlist->PathArcTo(center, r, 3.49f, 5.89f, 12);
			drawlist->PathStroke(iconColor, 0, 0.6f * p.unit);
			const float tip = 1.3f * p.unit;
			const ImVec2 endA(center.x + std::cos(2.75f) * r, center.y + std::sin(2.75f) * r);
			const ImVec2 endB(center.x + std::cos(5.89f) * r, center.y + std::sin(5.89f) * r);
			drawlist->AddTriangleFilled(ImVec2(endA.x - tip, endA.y - tip), ImVec2(endA.x + tip, endA.y - tip),
				ImVec2(endA.x, endA.y + tip), iconColor);
			drawlist->AddTriangleFilled(ImVec2(endB.x - tip, endB.y + tip), ImVec2(endB.x + tip, endB.y + tip),
				ImVec2(endB.x, endB.y - tip), iconColor);
		}
		p.popClip();
		return kPickerExtra * pickT;
	};

	const std::vector<ClickWindow*> hudPanels = hudMode ? std::vector<ClickWindow*>{ hudPanel } : std::vector<ClickWindow*>{};
	float hudPanelBodyH = 0.f;
	for (ClickWindow* window : hudMode ? hudPanels : windowList) {
		if (!window || (!hudMode && window->category == Category::Hud)) continue;
		const float x = window->pos.x;
		const float y = window->pos.y;
		const float w = kPanelWidth;

		const bool headerHovered = inside(mx, my, x, y, w, headerH);
		if (headerHovered && rightClick) {
			window->extended = !window->extended;
			click();
		}
		if (headerHovered && leftClick && !hachimiDragWindow)
			hachimiDragWindow = window;
		const float openT = stepAnim(hachimiPanelAnims[window], window->extended, 200.f, dt, easeCubicOut);
		const bool panelInteractive = window->extended && openT > 0.1f;

		float content = 1.5f;
		for (Module* mod : window->modList) {
			content += rowH + interval;
			ClickModuleState* state = window->getClickModule(mod->getModuleName());
			const float expandT = stepAnim(hachimiExpandAnims[mod], state->isExtended, 200.f, 0.f, easeCubicOut);
			if (expandT < 0.01f) continue;
			content += 2.f * expandT;
			for (Setting* setting : mod->getSettingList()) {
				if (!isVisible(setting)) continue;
				content += (rowH + interval + settingExtra(setting)) * expandT;
			}
		}

		if (hudMode) hudPanelBodyH = content * openT;
		const UIColor outlineColor = themeColor(y);
		if (openT > 0.01f) {
			const float bodyBottom = y + headerH + content * openT;
			p.pushClip(x, y + headerH, x + w, bodyBottom);
			p.fill(x + 1.5f - frameInset, y + headerH, w - 3.f + frameInset * 2.f, content - 1.5f + frameInset,
				theme.background);

			float cursor = y + headerH + interval;
			for (Module* mod : window->modList) {
					const float modX = x + 1.5f;
				const float modY = cursor;
				ClickModuleState* state = window->getClickModule(mod->getModuleName());
				const bool enabled = mod->isEnabled();
				const bool visibleRow = modY + rowH > y + headerH && modY < bodyBottom;
				const bool hovered = visibleRow && inside(mx, my, modX, modY, kModuleWidth, rowH)
					&& my >= y + headerH && my < bodyBottom;

				if (hovered && panelInteractive) {
					if (leftClick) {
						if (isShiftDown) mod->setDrawn(mod->isHiddenFromArrayList());
						else mod->toggle();
						CommandManager::markForSave();
						click();
					} else if (rightClick) {
						state->isExtended = !state->isExtended;
						click();
					}
					const std::string desc = queryModuleDescription(mod->getModuleName(), mod->getDescription());
					if (!desc.empty()) tooltip = desc;
				}

				const std::string name = display(queryModuleName(mod->getModuleName()));
				float nameReserve = 2.f;
				if (hachimiGear) nameReserve = (std::max)(nameReserve, p.width("*") + 5.f);
				if (isShiftDown) nameReserve = (std::max)(nameReserve, 12.f);
				const float nameMaxW = kModuleWidth - 3.f - nameReserve;
				UIColor buttonColor = kClear;
				if (theme.moduleButton) {
					buttonColor = stateColor(mod, enabled, hovered, modY);
					fadeFill(modX, modY, kModuleWidth, rowH, buttonColor);
					p.textShrink(name, modX + 3.f, modY + textPad, nameMaxW, hachimiGrayText && !enabled ? kGray : kWhite);
				} else {
					const UIColor target = enabled ? themeColor(modY) : (hachimiGrayText ? kGray : kWhite);
					const UIColor textColor = stepColor(hachimiColorAnims[&mod->getSettingList()], target, dt);
					const float lift = stepAnim(hachimiHoverAnims[mod], hovered, 50.f, dt, easeQuadOut);
					p.textShrink(name, modX + 3.f, modY + textPad - lift, nameMaxW, textColor);
				}
				if (isShiftDown) {
					const UIColor eyeColor = mod->isHiddenFromArrayList() ? UIColor(128, 128, 128, 255) : kWhite;
					const ImVec2 center = p.at(modX + kModuleWidth - 7.f, modY + rowH * 0.5f);
					const float r = 3.f * p.unit;
					drawlist->PathArcTo(ImVec2(center.x, center.y + r * 0.9f), r * 1.35f, -2.35f, -0.79f, 10);
					drawlist->PathArcTo(ImVec2(center.x, center.y - r * 0.9f), r * 1.35f, 0.79f, 2.35f, 10);
					drawlist->PathStroke(p.col(eyeColor), ImDrawFlags_Closed, 0.6f * p.unit);
					drawlist->AddCircleFilled(center, r * 0.38f, p.col(eyeColor));
				}
				if (hachimiGear) {
					const float gearW = p.width("*");
					p.text("*", modX + kModuleWidth - 4.f - gearW, modY + 1.f + textPad, kWhite);
				}

				float used = rowH + interval;
				const float expandT = stepAnim(hachimiExpandAnims[mod], state->isExtended, 200.f, dt, easeCubicOut);
				if (expandT > 0.01f) {
					const float settingsTop = modY + rowH + 0.5f;
					float settingsHeight = 0.f;
					for (Setting* setting : mod->getSettingList())
						if (isVisible(setting)) settingsHeight += rowH + interval + settingExtra(setting);
					const float revealBottom = settingsTop + (1.f + settingsHeight) * expandT;
					p.pushClip(modX, settingsTop - 1.f, modX + kModuleWidth, revealBottom);
					const bool settingsInteractive = state->isExtended && panelInteractive;

					float sy = settingsTop;
					for (Setting* setting : mod->getSettingList()) {
						if (!isVisible(setting)) continue;
						const float sx = modX + 1.f;
						const bool rowVisible = my >= y + headerH && my < bodyBottom && my < revealBottom;
						const bool sHovered = rowVisible && inside(mx, my, sx, sy, kSettingWidth, rowH);
						const auto translated = querySettingTranslation(mod->getModuleName(), setting->name,
							setting->name, setting->description);
						const std::string label = display(translated.first);
						if (sHovered && settingsInteractive && !translated.second.empty())
							tooltip = translated.second;

						switch (setting->type) {
						case SettingType::BOOL_S: {
							auto* bs = static_cast<BoolSetting*>(setting);
							if (sHovered && settingsInteractive && leftClick) {
								*bs->value = !*bs->value;
								CommandManager::markForSave();
								click();
							}
							fadeFill(sx, sy, kSettingWidth, rowH, stateColor(bs, *bs->value, sHovered, sy));
							p.text(p.fit(label, kSettingWidth - 4.f), sx + 2.f, sy + textPad, kWhite);
							break;
						}
						case SettingType::SLIDER_S: {
							auto* slider = static_cast<SliderSettingBase*>(setting);
							const bool editing = hachimiEditSlider == slider;
							if (sHovered && settingsInteractive) {
								if (editing) editHit = true;
								if (leftClick) {
									draggingSliderSettingPtr = slider;
									click();
								} else if (rightClick) {
									if (editing) {
										handleHachimiKey(VK_RETURN);
									} else {
										hachimiEditSlider = slider;
										hachimiEditBuffer = formatNumber(slider, decimals);
										editHit = true;
									}
									click();
								}
							}
							float fraction = 0.f;
							if (slider->valueType == ValueType::INT_T) {
								auto* s = static_cast<SliderSetting<int>*>(slider);
								if (draggingSliderSettingPtr == slider) {
									const float t = (mx - sx) / kSettingWidth;
									int value = static_cast<int>(std::clamp(s->minValue + t * (s->maxValue - s->minValue),
										static_cast<float>(s->minValue), static_cast<float>(s->maxValue)));
									if (mx < std::ceil(sx + 1.f)) value = s->minValue;
									else if (mx >= std::floor(sx + kSettingWidth - 1.f)) value = s->maxValue;
									if (*s->valuePtr != value) { *s->valuePtr = value; CommandManager::markForSave(); }
								}
								const float range = static_cast<float>(s->maxValue - s->minValue);
								fraction = range > 0.f ? (*s->valuePtr - s->minValue) / range : 0.f;
							} else {
								auto* s = static_cast<SliderSetting<float>*>(slider);
								if (draggingSliderSettingPtr == slider) {
									const float t = (mx - sx) / kSettingWidth;
									float value = std::clamp(s->minValue + t * (s->maxValue - s->minValue), s->minValue, s->maxValue);
									const float scale = std::pow(10.f, static_cast<float>(std::clamp(decimals, 1, 5)));
									value = std::round(value * scale) / scale;
									if (mx < std::ceil(sx + 1.f)) value = s->minValue;
									else if (mx >= std::floor(sx + kSettingWidth - 1.f)) value = s->maxValue;
									if (*s->valuePtr != value) { *s->valuePtr = value; CommandManager::markForSave(); }
								}
								const float range = s->maxValue - s->minValue;
								fraction = range > 0.f ? (*s->valuePtr - s->minValue) / range : 0.f;
							}
							if (!editing) {
								fadeFill(sx, sy, std::clamp(fraction, 0.f, 1.f) * kSettingWidth, rowH,
									stateColor(slider, true, sHovered, sy));
								const std::string value = formatNumber(slider, decimals);
								p.labelValue(label, sx + 2.f, value, sx + kSettingWidth - 2.f, sy + textPad, kWhite, kGray);
							} else {
								p.text(hachimiEditBuffer + caret(), sx + 2.f, sy + textPad, kWhite);
							}
							break;
						}
						case SettingType::ENUM_S: {
							auto* es = static_cast<EnumSetting*>(setting);
							const int count = static_cast<int>(es->enumList.size());
							bool& dropdown = hachimiDropdownOpen[es];
							if (sHovered && settingsInteractive && count > 0) {
								if (leftClick) {
									*es->value = (*es->value + 1) % count;
									CommandManager::markForSave();
									click();
								} else if (rightClick) {
									dropdown = !dropdown;
									click();
								}
							}
							const float listH = count * (p.lineHeight + 1.f) + 3.f;
							if (dropdown && settingsInteractive && leftClick && rowVisible
								&& inside(mx, my, sx, sy + rowH, kSettingWidth, listH - 1.f)) {
								for (int i = 0; i < count; ++i) {
									if (inside(mx, my, sx, sy + rowH + i * (p.lineHeight + 1.f), kSettingWidth, p.lineHeight + 1.f)) {
										*es->value = i;
										CommandManager::markForSave();
										break;
									}
								}
								click();
							}
							const UIColor rowColor = stateColor(es, true, sHovered, sy);
							fadeFill(sx, sy, kSettingWidth, rowH, rowColor);
							const int current = std::clamp(*es->value, 0, (std::max)(0, count - 1));
							const std::string value = count > 0 ? display(queryEnumValueTranslation(es->enumList[current])) : "";
							p.labelValue(label, sx + 2.f, value, sx + kSettingWidth - 2.f, sy + textPad, kWhite, kGray);
							const float dropT = stepAnim(hachimiDropdownAnims[es], dropdown, 200.f, dt, easeCubicOut);
							if (dropT > 0.01f) {
								const float shown = listH * dropT;
								p.pushClip(sx, sy + rowH, sx + kSettingWidth, sy + rowH + shown);
								p.fill(sx, sy + rowH, 0.5f, shown - 0.5f, rowColor);
								p.fill(sx + kSettingWidth - 0.5f, sy + rowH, 0.5f, shown - 0.5f, rowColor);
								p.fill(sx + 0.5f, sy + rowH + shown - 1.f, kSettingWidth - 1.f, 0.5f, rowColor);
								for (int i = 0; i < count; ++i)
									p.text(p.fit(display(queryEnumValueTranslation(es->enumList[i])), kSettingWidth - textPad - 2.f), sx + textPad,
										sy + rowH + 2.f + i * (p.lineHeight + 1.f), i == current ? kWhite : kGray);
								p.popClip();
								sy += shown;
							}
							break;
						}
						case SettingType::KEYBIND_S: {
							auto* ks = static_cast<KeybindSetting*>(setting);
							EnumSetting* activation = nullptr;
							for (Setting* other : mod->getSettingList())
								if (other->type == SettingType::ENUM_S && other->name == "Activation")
									activation = static_cast<EnumSetting*>(other);
							const bool capturing = capturingKbSettingPtr == ks;
							if (capturing && leftClick && !sHovered) capturingKbSettingPtr = nullptr;
							if (sHovered && settingsInteractive) {
								if (leftClick) {
									if (capturing) {
										capturingKbSettingPtr = nullptr;
									} else if (isShiftDown && activation) {
										*activation->value = *activation->value == 1 ? 0 : 1;
										CommandManager::markForSave();
									} else {
										capturingKbSettingPtr = ks;
									}
									click();
								} else if (middleClick) {
									*ks->value = 0;
									capturingKbSettingPtr = nullptr;
									refreshBinds();
									CommandManager::markForSave();
									click();
								}
							}
							fadeFill(sx, sy, kSettingWidth, rowH, stateColor(ks, false, sHovered, sy));
							{
								const ImVec2 a = p.at(sx + 2.f, sy + rowH * 0.5f - 3.f);
								const ImVec2 b = p.at(sx + 11.f, sy + rowH * 0.5f + 3.f);
								drawlist->AddRect(a, b, p.col(kWhite), 0.8f * p.unit, 0, 0.5f * p.unit);
								for (int row = 0; row < 2; ++row)
									for (int k = 0; k < 3; ++k) {
										const ImVec2 dot = p.at(sx + 3.8f + k * 2.6f, sy + rowH * 0.5f - 1.4f + row * 2.f);
										drawlist->AddRectFilled(dot, ImVec2(dot.x + 0.9f * p.unit, dot.y + 0.9f * p.unit), p.col(kWhite));
									}
							}
							std::string labelText = label;
							if (sHovered && isShiftDown && activation)
								labelText = *activation->value == 1 ? "Hold" : "Toggle";
							std::string value = capturingKbSettingPtr == ks ? "..." : keyName(*ks->value);
							if (value.size() != 1) value = display(value);
							p.labelValue(labelText, sx + 13.f, value, sx + kSettingWidth - 2.f, sy + textPad, kWhite, kGray);
							break;
						}
						case SettingType::COLOR_S: {
							const bool themeSetting = mod->getModuleName() == "Color"
								&& (setting->name == "Theme" || setting->name == "Color");
							sy += colorRow(static_cast<ColorSetting*>(setting), label, themeSetting, sx, sy,
								sHovered, rowVisible, settingsInteractive);
							break;
						}
						case SettingType::TEXT_S: {
							auto* ts = static_cast<TextSetting*>(setting);
							const bool editing = capturingTextSettingPtr == ts;
							if (editing && leftClick && !sHovered) {
								ts->editing = false;
								capturingTextSettingPtr = nullptr;
								CommandManager::markForSave();
							}
							if (sHovered && settingsInteractive && leftClick) {
								if (editing) {
									ts->editing = false;
									capturingTextSettingPtr = nullptr;
									CommandManager::markForSave();
								} else {
									if (capturingTextSettingPtr) capturingTextSettingPtr->editing = false;
									capturingTextSettingPtr = ts;
									ts->backupValue = ts->value ? *ts->value : "";
									ts->editBuffer = ts->backupValue;
									ts->editing = true;
								}
								click();
							}
							fadeFill(sx, sy, kSettingWidth, rowH, stateColor(ts, false, sHovered, sy));
							const std::string value = ts->value ? *ts->value : "";
							if (capturingTextSettingPtr == ts) {
								p.text(value + caret(), sx + 2.f, sy + textPad, kWhite);
							} else {
								p.text(label, sx + 2.f, sy + textPad, kWhite);
								p.text(value, sx + 2.f + p.width(label + " "), sy + textPad, kGray);
							}
							break;
						}
						default:
							break;
						}
						sy += rowH + interval;
					}

					if (theme.moduleButton) {
						const float linesTop = modY + rowH;
						p.fill(modX, linesTop, 0.5f, sy - linesTop + 0.5f, buttonColor);
						p.fill(modX + kModuleWidth - 0.5f, linesTop, 0.5f, sy - linesTop + 0.5f, buttonColor);
						p.fill(modX, sy + 0.5f, kModuleWidth, 0.5f, buttonColor);
					}
					p.popClip();
					used += (settingsHeight + 2.f) * expandT;
				} else if (hachimiEditSlider || hachimiHexSetting) {
					for (Setting* setting : mod->getSettingList()) {
						if (setting == hachimiEditSlider) { hachimiEditSlider = nullptr; hachimiEditBuffer.clear(); }
						if (setting == hachimiHexSetting) { hachimiHexSetting = nullptr; hachimiHexBuffer.clear(); }
					}
				}
				cursor += used;
			}
			p.popClip();

			const float bodyH = content * openT;
			if (!theme.slim) {
				const float side = 1.5f - frameInset;
				p.fill(x + w - side, y + headerH, side, bodyH, theme.frame);
				p.fill(x, y + headerH, side, bodyH, theme.frame);
				p.fill(x + 1.5f - frameInset, y + headerH + bodyH - side, w - 3.f + frameInset * 2.f, side, theme.frame);
			}
			if (theme.outline) {
				p.fill(x + w - slimInset, y + headerH, 0.5f, (content - slimInset) * openT, outlineColor);
				p.fill(x - 0.5f + slimInset, y + headerH, 0.5f, (content - slimInset) * openT, outlineColor);
			}
		}

		p.fill(x + slimInset, y, w - slimInset * 2.f, headerH, theme.frame);
		if (theme.outline) {
			p.fill(x + w - slimInset, y, 0.5f, headerH + 0.5f, outlineColor);
			p.fill(x - 0.5f + slimInset, y, 0.5f, headerH + 0.5f, outlineColor);
			p.fill(x - 0.5f + slimInset, y - 0.5f, w + 1.f - slimInset * 2.f, 0.5f, outlineColor);
			p.fill(x - 0.5f + slimInset, y + headerH + (content - slimInset) * openT, w + 1.f - slimInset * 2.f, 0.5f,
				outlineColor);
		}
		const std::string title = display(queryCategoryName(static_cast<int>(window->category), window->windowName));
		if (!theme.icon) {
			p.text(title, x + 5.f, y + 4.f, kWhite);
		} else {
			if (ImFont* iconFont = ClickGui::getCategoryIconFont()) {
				const float iconPixels = std::round(p.lineHeight * p.unit);
				const char* glyph = ClickGui::getCategoryIconGlyph(window->category);
				const ImVec2 size = iconFont->CalcTextSizeA(iconPixels, 256.f, -1.f, glyph);
				const ImVec2 anchor = p.at(x + 4.f, y + headerH * 0.5f);
				drawlist->AddText(iconFont, iconPixels, ImVec2(anchor.x, anchor.y - size.y * 0.5f), p.col(kWhite), glyph);
			}
			p.text(title, x + w - p.width(title) - 5.f, y + 4.f, kWhite);
		}
	}

	if (anyClick && !editHit) {
		hachimiEditSlider = nullptr;
		hachimiEditBuffer.clear();
		hachimiHexSetting = nullptr;
		hachimiHexBuffer.clear();
	}

	if (hudMode) {
		const bool overPanel = inside(mx, my, hudPanel->pos.x, hudPanel->pos.y, kPanelWidth, headerH + hudPanelBodyH);
		if (overPanel || hachimiDragWindow == hudPanel) {
			shouldToggleLeftClick = false;
			shouldToggleRightClick = false;
		}
		hudPanel->pos = Vec2<float>(hudPanel->pos.x * p.unit, hudPanel->pos.y * p.unit);
		hudPanel->renderWidth = kPanelWidth * p.unit;
		hudPanel->headerRenderHeight = headerH * p.unit;
		hudPanel->visibleHeight = hudPanelBodyH * p.unit;
	}

	p.offsetY = 0.f;
	if (hachimiDescriptions) {
		if (!tooltip.empty()) hachimiTooltipText = tooltip;
		const bool showTooltip = !tooltip.empty() && !isShiftDown && !draggingSliderSettingPtr && !draggingColorSettingPtr;
		const float t = stepAnim(hachimiTooltipAnim, showTooltip, 200.f, dt, easeLinear);
		if (t > 0.01f && !hachimiTooltipText.empty()) {
			const float tw = p.width(hachimiTooltipText);
			p.fill(mx + 10.f, my - 8.f, tw + 4.f, p.lineHeight + 2.f, UIColor(0, 0, 0, static_cast<uint8_t>(50 + 60 * t)));
			p.text(hachimiTooltipText, mx + 12.f, my - 7.f, UIColor(255, 255, 255, static_cast<uint8_t>(255 * t)));
		}
	}
}
