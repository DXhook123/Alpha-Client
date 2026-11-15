#pragma once
#include <string>
#include "../Utils/ColorUtil.h"
#include "../Utils/Maths.h"
#include "../Libs/imgui/imgui.h"

class ImGuiUtils {
private:
    static inline ImDrawList* drawlist = nullptr;
    static inline float fontSize = 25.f;
    static inline float fontScale = 1.f;
    static inline int fontStyle = 0;
    static inline float shadowOffset = 1.f;
    static inline ImFont* textFontOverride = nullptr;
public:
    static void setDrawList(ImDrawList* d);
    static ImDrawList* getDrawList() { return drawlist; }
    static ImFont* getInterfaceFont();
    static ImFont* getTextFontOverride() { return textFontOverride; }
    class ScopedFontOverride {
    public:
        explicit ScopedFontOverride(ImFont* font) : previous(textFontOverride) {
            textFontOverride = font;
        }
        ~ScopedFontOverride() { textFontOverride = previous; }
        ScopedFontOverride(const ScopedFontOverride&) = delete;
        ScopedFontOverride& operator=(const ScopedFontOverride&) = delete;
    private:
        ImFont* previous;
    };
    static void drawText(Vec2<float> textPos, const std::string& textStr, UIColor color, float textSize, bool glow = false, float thick_ness = 0.f);
    static void drawText(ImFont* font, Vec2<float> textPos, const std::string& textStr, UIColor color, float textSize, bool glow = false, float thick_ness = 0.f);
    static void drawTextGlow(Vec2<float> textPos, const std::string& textStr, UIColor color,
        float textSize, float strength = 1.f, float radius = 6.f);
    static void setFontSize(float newSize) { fontSize = newSize; };
    static void setFontScale(float scale) { fontScale = scale; }
    static float getFontSize() { return fontSize; }
    static float getFontScale() { return fontScale; }
    static float getReadableTextScale(float textSize, float minimumPixels = 13.f);
    static void setFontStyle(int style) { fontStyle = style >= 0 && style <= 2 ? style : 0; }
    static int getFontStyle() { return fontStyle; }
    static float getBakedTextScale(float textSize);
    static void setShadowOffset(float offset) { shadowOffset = offset; }
    static void drawMtfFlowText(float x, float y, const std::string& text,
        float textSize, int alpha, double timeSec, float flowSpeed = 1.f, float shadow = 0.f);
    static float getTextWidth(const std::string& textStr, float textSize = 1.f);
    static float getTextWidth(ImFont* font, const std::string& textStr, float textSize = 1.f);
    static float getTextHeight(float textSize = 1.f);
    static float getTextHeight(ImFont* font, float textSize = 1.f);
    static void drawRectangle(const Vec4<float>& rectPos, UIColor color, float lineWidth = 1.0f, float rounding = 0.0f, ImDrawFlags flags = 0);
    static void drawRectangle(Vec2<float> pMin, Vec2<float> pMax, UIColor color, float lineWidth = 1.0f, float rounding = 0.0f, ImDrawFlags flags = 0);
    static void drawRectangleShadow(const Vec4<float>& rectPos, UIColor color, float shadow_thickness, const ImVec2& shadow_offset, float rounding, ImDrawFlags flags = 0);
    static void fillRectangle(Vec4<float> rectPos, UIColor color, float rounding = 0.0f, ImDrawFlags flags = 0);
    static void fillRectangle(Vec2<float> pMin, Vec2<float> pMax, UIColor color, float rounding = 0.0f, ImDrawFlags flags = 0);
    static void drawBox(const Vec3<float>& blockPos, UIColor color, UIColor lineColor, float lineWidth, bool fill, bool outline);
    static void drawBox(const AABB& blockAABB, UIColor color, UIColor lineColor, float lineWidth, bool fill, bool outline);
    static void renderCircle(Vec2<float> pos, ImColor color, float radius = 1.f, float thickness = 1.f, int num_seg = 0);
    static void renderCircleFilled(Vec2<float> pos, ImColor color, float radius = 1.f, int num_seg = 0);
    static void renderCircleShadow(Vec2<float> pos, ImColor color, float radius = 1.f, float thickness = 5.f, bool fill = false, int num_seg = 12, ImVec2 offset = ImVec2(0, 0));
    static void fillRoundedRectangle(const Vec4<float>& rectPos, UIColor color, float rounding, ImDrawFlags flags = 0);
    static void fillRoundedRectangle(Vec2<float> pMin, Vec2<float> pMax, UIColor color, float rounding, ImDrawFlags flags = 0);

    static void drawRoundedRectangle(const Vec4<float>& rectPos, UIColor color, float thickness, float rounding, ImDrawFlags flags = 0);
    static void drawRoundedRectangle(Vec2<float> pMin, Vec2<float> pMax, UIColor color, float thickness, float rounding, ImDrawFlags flags = 0);

    static void fillCircle(const Vec2<float>& center, float radius, const UIColor& color, int num_seg = 0);
    static void drawDashedLine(Vec2<float> start, Vec2<float> end, UIColor color, float thickness, float dashLength, float gapLength);
    static void fillCircle(Vec2<float> center, ImColor color, float radius, int num_seg = 0);

    static void drawCircle(const Vec2<float>& center, float radius, const UIColor& color, float thickness = 1.0f, int num_seg = 0);
    static void drawCircle(Vec2<float> center, ImColor color, float radius, float thickness = 1.0f, int num_seg = 0);

    static void fillGradientRectangle(const Vec4<float>& rectPos, UIColor colorTopLeft, UIColor colorTopRight, UIColor colorBottomLeft, UIColor colorBottomRight);

    static void drawDashedRectangle(const Vec4<float>& rectPos, UIColor color, float thickness, float dashLength, float gapLength, float rounding = 0.0f);

    static void drawDashedLineEx(Vec2<float> start, Vec2<float> end, UIColor color, float thickness, float dashLength, float gapLength);

    static void fillTriangle(Vec2<float> p1, Vec2<float> p2, Vec2<float> p3, UIColor color);
    static void drawTriangle(Vec2<float> p1, Vec2<float> p2, Vec2<float> p3, UIColor color, float thickness = 1.0f);

    static void fillRectangleAnimated(const Vec4<float>& rectPos, UIColor color, float rounding, float alphaMultiplier);
    static void drawRectangleAnimated(const Vec4<float>& rectPos, UIColor color, float thickness, float rounding, float alphaMultiplier);

    static void drawTooltip(Vec2<float> pos, const std::string& text, UIColor bgColor, UIColor textColor, float padding = 4.0f);
};
