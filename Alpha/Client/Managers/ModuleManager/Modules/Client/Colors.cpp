#include "ColorModule.h"
#include "Utils/ThemeColorMath.h"

UIColor ColorModule::getColor(double delay) const {
	if (colorMode != 0) {
		const auto rgb = ThemeColorMath::sample(colorMode, double(ColorUtil::getCurrentMs()),
			delay * (std::clamp)(rainbowDelay, 0, 1000), rainbowSpeed, saturation / 255.f,
			{ waveColor1.r, waveColor1.g, waveColor1.b }, { waveColor2.r, waveColor2.g, waveColor2.b });
		return UIColor(rgb.r, rgb.g, rgb.b, 255);
	}
	switch (presetMode) {
	case 1: return UIColor(91, 206, 250, 255);
	case 2: return UIColor(255, 209, 220, 255);
	case 3: return UIColor(230, 220, 250, 255);
	case 4: return UIColor(185, 103, 255, 255);
	default: return topColor;
	}
}

UIColor ColorModule::resolveColor(const UIColor& custom, double offset) {
	auto* cm = instance();
	if (!cm) return custom;
	if (cm->colorMode != 0) {
		UIColor color = cm->getColor(offset);
		color.a = custom.a;
		return color;
	}
	if (cm->presetMode != 0) {
		UIColor color = getThemeColor();
		color.a = custom.a;
		return color;
	}
	return custom;
}
