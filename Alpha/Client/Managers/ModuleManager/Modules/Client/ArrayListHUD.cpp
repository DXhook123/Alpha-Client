#include "ArrayListHUD.h"
#include "SettingsModule.h"
#include "Renderer/ImGuiUtils.h"
#include "Utils/ColorUtil.h"
#include "Utils/MemoryUtil.h"
#include "sdk/font/McrRender.h"
#include "../../ModuleManager.h"
#include "../ModuleTranslations.hpp"
#include "ClickGui.h"
#include "ColorModule.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
    float animate(float current,bool enabled,float dt) {
        const float target=enabled ? 1.f:0.f;
        float result=target+(current-target)*std::pow(0.8f,dt*60.f);
        return std::abs(result-target)<.001f ? target:result;
    }
    void lowerAscii(std::string& text) {
        for (char& ch:text) if (ch>='A' && ch<='Z') ch+=char('a'-'A');
    }
    std::string keyName(int key) {
        const size_t count=sizeof(KeyNames)/sizeof(KeyNames[0]);
        if (key>0 && size_t(key)<count && KeyNames[key] && *KeyNames[key]) return KeyNames[key];
        return key>0 ? "VK_"+std::to_string(key):std::string{};
    }
    UIColor faded(UIColor color,float progress) {
        color.a=uint8_t(std::clamp(color.a*progress,0.f,255.f));
        return color;
    }

	float approach(float current, float target, float speed, float deltaTime) {
		return current + (target - current) * (1.f - std::exp(-speed * deltaTime));
	}

	float clampf(float value, float minValue, float maxValue) {
		return (std::max)(minValue, (std::min)(value, maxValue));
	}

	float easeInOutQuad(float value) {
		value = clampf(value, 0.f, 1.f);
		return value < .5f ? 2.f * value * value
			: 1.f - std::pow(-2.f * value + 2.f, 2.f) * .5f;
	}

	constexpr float epsilonAnimationDuration = .2f;
}

ArrayListHUD::ArrayListHUD()
    : HudModule("ArrayList", "Nuke-style animated module list", Category::Hud) {
    registerSetting(new EnumSetting("Layout", "Choose between the new Nuke list and the classic list",
        {"Nuke", "Classic"}, &layoutMode, 0));
    registerSetting(new EnumSetting("Style", "Module list border style",
        {"Outline", "Full", "Split", "Bar", "Right", "None", "Semi Bar", "Box"}, &style, 0));
    registerSetting(new BoolSetting("Modes", "Show module mode/status suffixes", &modes, true));
    registerSetting(new BoolSetting("Show Keybind", "Show bound keys after module names", &showKeybind, true));
    registerSetting(new BoolSetting("Lowercase", "Use lowercase Latin module names", &lowercase, false));
    registerSetting(new BoolSetting("Bottom", "Stack the list upward from the bottom", &bottom, false));
    registerSetting(new BoolSetting("Background", "Draw black backgrounds behind the entries", &background, true));
    registerSetting(new BoolSetting("Right Bar", "Show the right edge in bar styles", &rightBar, true,
        [this] { return style==1 || style==3 || style==4 || style==6; }));
    registerSetting(new SliderSetting<float>("Spacing", "Vertical padding around each entry", &spacing, 0.f, 0.f, 10.f));
    registerSetting(new BoolSetting("Shadow", "Shadow behind list text", &shadow, true));
    registerSetting(new SliderSetting<float>("Shadow Strength", "Opacity of the text shadow", &shadowStrength, .75f, 0.f, 1.f,
        [this] { return shadow; }));
    registerSetting(new BoolSetting("Glow", "Fixed-color glow behind text", &glow, true));
    registerSetting(new SliderSetting<float>("Glow Strength", "Opacity of the text glow", &glowStrength, .9f, 0.f, 1.f,
        [this] { return glow; }));
    registerSetting(new SliderSetting<float>("Glow Size", "Spread of the text glow", &glowSize, 3.5f, .75f, 8.f,
        [this] { return glow; }));
    opacity=150;
    anchoredRight=true;
    defaultPos=Vec2<float>(100000.f,3.f);
    position=defaultPos;
}

bool ArrayListHUD::isListedModule(const Module* module) const {
    if (!module || module==this || module->isHiddenFromArrayList()) return false;
    const auto& name=module->getModuleName();
    return name!="HudEditor" && name!="ClickGui" && name!="Color" && name!="Settings" && name!="CustomFont";
}
void ArrayListHUD::drawTextPart(const Vec2<float>& at,const std::string& text,
    const UIColor& color,float scale) {
    if (text.empty()) return;
    if (McrRender::isNativeHudEnabled()) {
        drawHudText(at,text,color,scale);
        return;
    }
    const int firstVertex=activeDrawList ? activeDrawList->VtxBuffer.Size:0;
    const bool transform=activeDrawList && textRatio!=1.f;
    if (transform) {
        const ImVec2 lo=activeDrawList->GetClipRectMin(),hi=activeDrawList->GetClipRectMax();
        const float inverse=1.f/(std::max)(textRatio,.001f);
        activeDrawList->PushClipRect(
            {(std::min)(lo.x,at.x+(lo.x-at.x)*inverse),(std::min)(lo.y,at.y+(lo.y-at.y)*inverse)},
            {(std::max)(hi.x,at.x+(hi.x-at.x)*inverse),(std::max)(hi.y,at.y+(hi.y-at.y)*inverse)},false);
    }
    if (shadow) ImGuiUtils::drawText(at.add(Vec2<float>(1.f,1.f)),text,
        UIColor(0,0,0,uint8_t(color.a*shadowStrength)),rasterScale,false);
    if (glow) ImGuiUtils::drawTextGlow(at,text,color,rasterScale,glowStrength,glowSize);
    drawHudText(at,text,color,rasterScale);
    if (transform) {
        for (int i=firstVertex;i<activeDrawList->VtxBuffer.Size;++i) {
            auto& p=activeDrawList->VtxBuffer[i].pos;
            p.x=at.x+(p.x-at.x)*textRatio;
            p.y=at.y+(p.y-at.y)*textRatio;
        }
        activeDrawList->PopClipRect();
    }
}

void ArrayListHUD::onImGuiRender(ImDrawList* drawlist) {
    if (layoutMode == 1) {
        renderClassic(drawlist);
        return;
    }
    activeDrawList=drawlist;
    setChineseMode(SettingsModule::getLanguage()==1);
    ImGuiIO& io=ImGui::GetIO();
    if (!std::isfinite(io.DisplaySize.x) || !std::isfinite(io.DisplaySize.y) ||
        io.DisplaySize.x<=1 || io.DisplaySize.y<=1) return;
    const float dt=std::isfinite(io.DeltaTime) ? std::clamp(io.DeltaTime,0.f,.1f):0.f;
    const float requested=std::clamp(size,.5f,4.f);
    if (animatedSize<=0.f) animatedSize=requested;
    animatedSize+=(requested-animatedSize)*(1.f-std::exp(-14.f*dt));
    if (std::abs(requested-animatedSize)<.0001f) animatedSize=requested;
    const float scale=animatedSize;
    const bool minecraftFont=McrRender::useMinecraftFont();
    rasterScale=minecraftFont ? scale:ImGuiUtils::getBakedTextScale(requested);
    textRatio=minecraftFont ? 1.f:scale/(std::max)(rasterScale,.001f);
    const float measureScale=minecraftFont ? requested:rasterScale;
    const float measureRatio=minecraftFont ? scale/requested:textRatio;
    const float textHeight=ImGuiUtils::getTextHeight(measureScale)*measureRatio;
    const float padX=3.f*scale, padY=(std::max)(0.f,spacing)*scale;
    const float rowHeight=textHeight+padY*2.f,edge=(std::max)(1.f,scale);

    const auto& modules=ModuleManager::getModules();
    for (Module* module:modules) {
        if (!isListedModule(module)) continue;
        if (!module->isEnabled()) continue;
        auto& entry=entries[module];
        entry.module=module;
        entry.name=queryModuleName(module->getModuleName());
        entry.suffix.clear();
        if (modes) {
            const auto status=module->getModeText();
            if (!status.empty()) entry.suffix=" ["+status+"]";
        }
        if (showKeybind && module->getKeybind()!=0) entry.suffix+=" ["+keyName(module->getKeybind())+"]";
    }
    std::vector<Entry*> rows;
    rows.reserve(entries.size());
    for (auto it=entries.begin();it!=entries.end();) {
        if (std::find(modules.begin(),modules.end(),it->first)==modules.end() ||
            !isListedModule(it->first)) { it=entries.erase(it); continue; }
        auto& entry=it->second;
        entry.progress=animate(entry.progress,entry.module->isEnabled(),dt);
        if (!entry.module->isEnabled() && entry.progress==0.f) { it=entries.erase(it); continue; }
        std::string name=entry.name,suffix=entry.suffix;
        if (lowercase) { lowerAscii(name); lowerAscii(suffix); }
        entry.nameWidth=hudTextWidth(name,measureScale)*measureRatio;
        entry.suffixWidth=suffix.empty() ? 0.f:hudTextWidth(suffix,measureScale)*measureRatio;
        if (entry.progress>.001f) rows.push_back(&entry);
        ++it;
    }
    std::sort(rows.begin(),rows.end(),[](const Entry* a,const Entry* b) {
        if (a->width()!=b->width()) return a->width()>b->width();
        if (a->name!=b->name) return a->name<b->name;
        return a->module->getModuleName()<b->module->getModuleName();
    });
    float widest=0,totalHeight=0;
    for (const auto* row:rows) {
        widest=(std::max)(widest,row->width()+padX*2.f+edge*2.f);
        totalHeight+=rowHeight*row->progress;
    }
    if (rows.empty()) {
        if (isEditing()) {
            Vec2<float> anchor=getClampedRenderPosition(110.f,rowHeight);
            drawEditorRect({anchor.x-110.f,anchor.y,anchor.x,anchor.y+rowHeight});
        }
        return;
    }
    const float margin=3.f;
    const float visibleHeight=(std::min)(totalHeight,(std::max)(1.f,io.DisplaySize.y-margin*2.f));
    if ((layoutInitialized && bottom!=previousBottom) ||
        (!layoutInitialized && bottom && position.y<=margin))
        position.y=(std::max)(margin,io.DisplaySize.y-position.y-visibleHeight);
    else if (bottom && layoutInitialized && !isDragging && !reflowFontLayout)
        position.y+=previousHeight-visibleHeight;
    previousHeight=visibleHeight;
    previousBottom=bottom;
    layoutInitialized=true;
    Vec2<float> anchor=getClampedRenderPosition(widest,visibleHeight);
    anchor.x=(std::min)(anchor.x,io.DisplaySize.x-margin);
    const float top=anchor.y;
    const float right=anchor.x;
    float cursor=bottom ? top+visibleHeight:top;
    struct Row { Entry* entry; Vec4<float> rect; UIColor accent; };
    std::vector<Row> layout;
    layout.reserve(rows.size());
    int colorIndex=0;
    for (Entry* entry:rows) {
        const float height=rowHeight*entry->progress;
        const float width=entry->width()+padX*2.f+edge*2.f;
        const float left=right-width*entry->progress;
        const float y=bottom ? cursor-height:cursor;
        const UIColor accent=getHudColor(labelColor,colorIndex++ * .1);
        layout.push_back({entry,{left,y,left+width,y+height},faded(accent,entry->progress)});
        cursor+=bottom ? -height:height;
    }
    for (size_t i=0;i<layout.size();++i) {
        const auto& row=layout[i]; const auto& r=row.rect; const auto& entry=*row.entry;
        if (r.w<top || r.y>top+visibleHeight) continue;
        if (background) ImGuiUtils::fillRectangle(r,UIColor(0,0,0,
            uint8_t(std::clamp(opacity*entry.progress,0.f,255.f))));
        auto rect=[&](float x1,float y1,float x2,float y2) {
            if (x2>x1 && y2>y1) ImGuiUtils::fillRectangle({x1,y1,x2,y2},row.accent);
        };
        if (style==0 || style==1 || style==2) rect(r.x,r.y,r.x+edge,r.w);
        if ((style==1 || style==3 || style==4) && rightBar) rect(r.z-edge,r.y,r.z,r.w);
        if (style==6 && rightBar) {
            const float inset=(std::min)(3.f*scale,(r.w-r.y)*.25f);
            ImGuiUtils::fillRectangle({r.z-edge*2.f,r.y+inset,r.z,r.w-inset},row.accent,edge);
        }
        if (style==7) ImGuiUtils::drawRectangle(r,row.accent,edge);
        if (style==0 || style==1) {
            const float boundary=bottom ? r.y:r.w;
            if (i+1<layout.size()) {
                const float nextLeft=layout[i+1].rect.x;
                rect((std::min)(r.x,nextLeft),boundary-edge*.5f,
                    (std::max)(r.x,nextLeft)+edge,boundary+edge*.5f);
            } else rect(r.x,boundary-edge*.5f,r.z,boundary+edge*.5f);
            if (style==1 && i==0) {
                const float cap=bottom ? r.w:r.y;
                rect(r.x,cap-edge*.5f,r.z,cap+edge*.5f);
            }
        }
        if (style==4 && i==0) {
            const float cap=bottom ? r.w:r.y;
            rect(r.x,cap-edge*.5f,r.z,cap+edge*.5f);
        }
        const Vec2<float> textAt(r.x+padX+edge,r.y+(r.w-r.y-textHeight)*.5f);
        std::string name=entry.name,suffix=entry.suffix;
        if (lowercase) { lowerAscii(name); lowerAscii(suffix); }
        drawTextPart(textAt,name,row.accent,scale);
        drawTextPart(textAt.add(Vec2<float>(entry.nameWidth,0)),suffix,row.accent,scale);
    }
    drawEditorRect({right-widest,top,right,top+visibleHeight});
}

void ArrayListHUD::renderClassic(ImDrawList* drawlist) {
	setChineseMode(SettingsModule::getLanguage() == 1);
	ImGuiIO& io = ImGui::GetIO();
	if (io.DisplaySize.x <= 1.f || io.DisplaySize.y <= 1.f) return;
	const float dt = (std::max)(0.001f, (std::min)(io.DeltaTime, 0.1f));
	const float requestedTextScale = clampf(size, 0.5f, 4.f);
	const float textScale = McrRender::useMinecraftFont()
		? requestedTextScale : ImGuiUtils::getBakedTextScale(requestedTextScale);
	const float textHeight = ImGuiUtils::getTextHeight(textScale);
	const float rowHeight = textHeight + (std::max)(0.f, spacing);
	const float prefixW = 0.f;

	for (Module* module : ModuleManager::getModules()) {
		if (!isListedModule(module)) continue;
		auto found = std::find_if(legacyEntries.begin(), legacyEntries.end(), [module](const LegacyEntry& entry) {
			return entry.module == module;
		});
		if (found == legacyEntries.end()) {
			LegacyEntry entry;
			entry.module = module;
			entry.name = queryModuleName(module->getModuleName());
			legacyEntries.push_back(entry);
		}
	}

	for (LegacyEntry& entry : legacyEntries) {
		const bool enabled = entry.module && entry.module->isEnabled() && isListedModule(entry.module);
		if (entry.module) {
			entry.name = queryModuleName(entry.module->getModuleName());
			if (enabled) {
				const std::string status = entry.module->getModeText();
				if (!status.empty()) entry.name += "[" + status + "]";
			}
		}
		const float targetWidth = enabled ? hudTextWidth(entry.name, textScale) + prefixW : 0.f;
		entry.width = approach(entry.width, targetWidth, 15.f, dt);
		entry.alpha = approach(entry.alpha, enabled ? 1.f : 0.f, 13.f, dt);
	}

	std::sort(legacyEntries.begin(), legacyEntries.end(), [](const LegacyEntry& left, const LegacyEntry& right) {
		return left.width > right.width;
	});

	legacyEntries.erase(std::remove_if(legacyEntries.begin(), legacyEntries.end(), [](const LegacyEntry& entry) {
		return (!entry.module || entry.alpha < 0.01f) && entry.width < 0.1f;
	}), legacyEntries.end());

	float widest = 0.f;
	float renderedHeight = 0.f;
	for (const LegacyEntry& entry : legacyEntries) {
		if (entry.alpha < 0.01f) continue;
		widest = (std::max)(widest, entry.width);
		renderedHeight += rowHeight * entry.alpha;
	}
	if (widest < 0.1f) return;

	const float padding = 4.f;
	const float totalWidth = widest + padding * 2.f;
	Vec2<float> renderPosition = getClampedRenderPosition(totalWidth, renderedHeight);
	renderPosition.x = (std::min)(renderPosition.x, io.DisplaySize.x - 3.f);
	const Vec4<float> editorRect(renderPosition.x - totalWidth, renderPosition.y,
		renderPosition.x, renderPosition.y + renderedHeight);

	float y = renderPosition.y;
	int colorIndex = 0;
	for (const LegacyEntry& entry : legacyEntries) {
		if (entry.alpha < 0.01f) continue;
		const float rowH = rowHeight * entry.alpha;
		const float right = renderPosition.x;
		const float left = right - entry.width - padding * 2.f;
		const Vec4<float> row(left, y, right, y + rowH);
		const UIColor accent = ColorUtil::getWaveColor(ColorModule::getThemeColor(), labelColor, colorIndex * 36);
		const uint8_t alpha = static_cast<uint8_t>(clampf(entry.alpha * 255.f, 0.f, 255.f));
		if (background)
			ImGuiUtils::fillRectangle(row, UIColor(8, 9, 12, static_cast<uint8_t>(alpha * opacity / 255.f)), 0.f);
		if (rightBar)
			ImGuiUtils::fillRectangle(Vec4<float>(right - 1.5f, y, right, y + rowH), UIColor(accent.r, accent.g, accent.b, static_cast<uint8_t>(alpha * labelColor.a / 255.f)));
		drawHudText(Vec2<float>(right - padding - entry.width + prefixW, y), entry.name,
			UIColor(accent.r, accent.g, accent.b, static_cast<uint8_t>(alpha * labelColor.a / 255.f)), textScale);
		y += rowH;
		++colorIndex;
	}

	drawEditorRect(editorRect);
}

void ArrayListHUD::renderEpsilonStyle(float deltaTime) {
	setChineseMode(SettingsModule::getLanguage() == 1);
	ImGuiIO& io = ImGui::GetIO();
	if (io.DisplaySize.x <= 1.f || io.DisplaySize.y <= 1.f) return;

	const float requestedTextScale = clampf(size, 0.5f, 4.f);
	const float textScale = McrRender::useMinecraftFont()
		? requestedTextScale : ImGuiUtils::getBakedTextScale(requestedTextScale);
	const float textHeight = ImGuiUtils::getTextHeight(textScale);
	const float horizontalPadding = 3.f;
	const float verticalPadding = (std::max)(1.f, spacing);
	const float lineHeight = textHeight + verticalPadding * 2.f;
	const float now = static_cast<float>(ImGui::GetTime());
	const float prefixW = 0.f;

	for (Module* module : ModuleManager::getModules()) {
		if (!isListedModule(module)) continue;
		auto found = std::find_if(legacyEntries.begin(), legacyEntries.end(), [module](const LegacyEntry& entry) {
			return entry.module == module;
		});
		if (found == legacyEntries.end()) {
			LegacyEntry entry;
			entry.module = module;
			entry.name = queryModuleName(module->getModuleName());
			legacyEntries.push_back(std::move(entry));
		}
	}

	for (LegacyEntry& entry : legacyEntries) {
		const bool enabled = entry.module && entry.module->isEnabled() && isListedModule(entry.module);
		if (entry.module) {
			entry.name = queryModuleName(entry.module->getModuleName());
			if (enabled) {
				const std::string status = entry.module->getModeText();
				if (!status.empty()) entry.name += "[" + status + "]";
			}
		}

		entry.width = hudTextWidth(entry.name, textScale) + prefixW;
		const float target = enabled ? 1.f : 0.f;
		if (!entry.epsilonInitialized) {
			entry.epsilonInitialized = true;
			entry.epsilonTargetVisible = enabled;
			entry.epsilonFrom = 0.f;
			entry.epsilonTo = target;
			entry.epsilonStartedAt = now;
		}
		else if (entry.epsilonTargetVisible != enabled) {
			entry.epsilonTargetVisible = enabled;
			entry.epsilonFrom = entry.epsilonProgress;
			entry.epsilonTo = target;
			entry.epsilonStartedAt = now;
		}

		const float progress = clampf((now - entry.epsilonStartedAt) / epsilonAnimationDuration, 0.f, 1.f);
		entry.epsilonProgress = entry.epsilonFrom +
			(entry.epsilonTo - entry.epsilonFrom) * easeInOutQuad(progress);
		entry.alpha = entry.epsilonProgress;
	}

	std::sort(legacyEntries.begin(), legacyEntries.end(), [](const LegacyEntry& left, const LegacyEntry& right) {
		return left.width > right.width;
	});

	float widest = 0.f;
	float renderedHeight = 0.f;
	for (const LegacyEntry& entry : legacyEntries) {
		if (entry.epsilonProgress <= .001f) continue;
		widest = (std::max)(widest, entry.width);
		renderedHeight += lineHeight * entry.epsilonProgress;
	}
	if (widest < .1f || renderedHeight < .1f) return;

	const float totalWidth = widest + horizontalPadding * 2.f;
	const Vec2<float> renderPosition = getClampedRenderPosition(totalWidth, renderedHeight);
	const bool rightSide = anchoredRight ||
		renderPosition.x + totalWidth * .5f > io.DisplaySize.x * .5f;
	const float finalLeft = rightSide ? renderPosition.x - totalWidth : renderPosition.x;
	const float finalRight = finalLeft + totalWidth;

	float y = renderPosition.y;
	int colorIndex = 0;
	for (const LegacyEntry& entry : legacyEntries) {
		const float animation = entry.epsilonProgress;
		if (animation <= .001f) continue;

		const float rowWidth = entry.width + horizontalPadding * 2.f;
		const float slideOffset = rowWidth * (1.f - animation);
		const float rowLeft = rightSide
			? finalRight - rowWidth + slideOffset
			: finalLeft - slideOffset;
		const float rowHeight = lineHeight * animation;
		const Vec4<float> row(rowLeft, y, rowLeft + rowWidth, y + rowHeight);
		const UIColor accent = ColorUtil::getWaveColor(ColorModule::getThemeColor(), labelColor, colorIndex * 36);
		const uint8_t textAlpha = static_cast<uint8_t>(clampf(animation * labelColor.a, 0.f, 255.f));
		const Vec4<float> bar = rightSide
			? Vec4<float>(row.z - 2.f, row.y, row.z, row.w)
			: Vec4<float>(row.x, row.y, row.x + 2.f, row.w);

		if (background) {
			const uint8_t bgAlpha = static_cast<uint8_t>(clampf(animation * opacity, 0.f, 255.f));
			ImGuiUtils::fillRectangle(row, UIColor(8, 9, 12, bgAlpha), 0.f);
		}
		if (rightBar) {
			ImGuiUtils::fillRectangle(bar, UIColor(accent.r, accent.g, accent.b, textAlpha), 0.f);
		}

		const float textX = rightSide
			? row.z - horizontalPadding - entry.width + prefixW
			: row.x + horizontalPadding + prefixW;
		if (shadow && !McrRender::isNativeHudEnabled()) {
			const uint8_t textShadowAlpha = static_cast<uint8_t>(clampf(animation * 230.f, 0.f, 255.f));
			ImGuiUtils::drawText(Vec2<float>(textX + 1.25f,
				y + (rowHeight - textHeight) * .5f + 1.25f), entry.name,
				UIColor(static_cast<uint8_t>(accent.r * .03f), static_cast<uint8_t>(accent.g * .03f),
					static_cast<uint8_t>(accent.b * .03f), textShadowAlpha), textScale, false);
		}
		if (glow && !McrRender::isNativeHudEnabled()) {
			ImGuiUtils::drawTextGlow(Vec2<float>(textX,
				y + (rowHeight - textHeight) * .5f), entry.name,
				UIColor(accent.r, accent.g, accent.b, textAlpha), textScale,
				glowStrength, glowSize);
		}
		drawHudText(Vec2<float>(textX, y + (rowHeight - textHeight) * .5f), entry.name,
			UIColor(accent.r, accent.g, accent.b, textAlpha), textScale);
		y += rowHeight;
		++colorIndex;
	}

	drawEditorRect(Vec4<float>(finalLeft, renderPosition.y, finalRight,
		renderPosition.y + renderedHeight));
}
