#pragma once
#include "HudModule.h"
#include <unordered_map>
#include <vector>

class ArrayListHUD : public HudModule {
public:
    ArrayListHUD();
    void onImGuiRender(ImDrawList* drawlist) override;
    float size = 1.f;
private:
    int layoutMode = 0;
    int style = 0;
    bool modes = true, showKeybind = true, lowercase = false, bottom = false;
    bool background = true, rightBar = true, shadow = true, glow = true;
    float spacing = 0.f, shadowStrength = .75f, glowStrength = .9f, glowSize = 3.5f;
    struct Entry {
        Module* module = nullptr;
        std::string name, suffix;
        float nameWidth = 0, suffixWidth = 0, progress = 0;
        float width() const { return nameWidth + suffixWidth; }
    };
    std::unordered_map<Module*, Entry> entries;
    struct LegacyEntry {
        Module* module = nullptr;
        std::string name;
        float width = 0.f, alpha = 0.f;
        bool epsilonInitialized = false, epsilonTargetVisible = false;
        float epsilonFrom = 0.f, epsilonTo = 0.f, epsilonProgress = 0.f;
        float epsilonStartedAt = 0.f;
    };
    std::vector<LegacyEntry> legacyEntries;
    bool layoutInitialized = false, previousBottom = false;
    float previousHeight = 0.f;
    float animatedSize = 0.f;
    float rasterScale = 1.f, textRatio = 1.f;
    ImDrawList* activeDrawList = nullptr;
    bool isListedModule(const Module*) const;
    void drawTextPart(const Vec2<float>&, const std::string&, const UIColor&, float);
    void renderClassic(ImDrawList* drawlist);
    void renderEpsilonStyle(float deltaTime);
};
