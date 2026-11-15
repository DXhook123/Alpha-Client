#include "CustomFont.h"
#include "../Settings/EnumSetting.h"
#include "../Settings/BoolSetting.h"
#include "../Settings/SliderSetting.h"

CustomFont::CustomFont() : Module("CustomFont", "Changes the current client font.", Category::CLIENT) {
	registerSetting(new EnumSetting("Font", "Font used for Latin characters; NotoSans/MiSans support Chinese fallback",
		{ "Mojangles HD", "NotoSans", "WenKai Lite", "MiSans" }, &fontType, 0));
	registerSetting(new EnumSetting("Style", "Style of client text rendered by ImGuiUtils",
		{ "PLAIN", "BOLD", "ITALIC" }, &fontStyle, 0));
	registerSetting(new EnumSetting("Size Preset", "Compact readable font size",
		{ "Clear", "Medium", "Big", "Small" }, &sizePreset, 2));
	registerSetting(new BoolSetting("Shadow", "Draw a shadow behind text", &fontShadow, true));
	registerSetting(new SliderSetting<float>("Shadow Size", "Offset of the shadow behind all client text",
		&shadowSize, 1.f, 0.5f, 5.f));
	registerSetting(new SliderSetting<float>("Font Size", "Text scale for HUD and non-Epsilon overlays", &fontSize, 1.f, 0.8f, 2.f));
}
