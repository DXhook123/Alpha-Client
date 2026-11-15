#include "ColorUtil.h"

#include <math.h>
#include <sstream>
#include <iomanip>

UIColor ColorUtil::lerp(const UIColor& start, const UIColor& end, float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	return UIColor(
		static_cast<uint8_t>(start.r + (end.r - start.r) * t),
		static_cast<uint8_t>(start.g + (end.g - start.g) * t),
		static_cast<uint8_t>(start.b + (end.b - start.b) * t),
		static_cast<uint8_t>(start.a + (end.a - start.a) * t)
	);
}

UIColor ColorUtil::lerpHSV(const UIColor& start, const UIColor& end, float t) {
	t = std::clamp(t, 0.0f, 1.0f);

	float h1, s1, v1, h2, s2, v2;
	ImGui::ColorConvertRGBtoHSV(start.r / 255.0f, start.g / 255.0f, start.b / 255.0f, h1, s1, v1);
	ImGui::ColorConvertRGBtoHSV(end.r / 255.0f, end.g / 255.0f, end.b / 255.0f, h2, s2, v2);

	float dh = h2 - h1;
	if (dh > 0.5f) dh -= 1.0f;
	if (dh < -0.5f) dh += 1.0f;
	float h = h1 + dh * t;
	if (h < 0.0f) h += 1.0f;
	if (h >= 1.0f) h -= 1.0f;

	float s = s1 + (s2 - s1) * t;
	float v = v1 + (v2 - v1) * t;

	float r, g, b;
	ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);

	return UIColor(
		static_cast<uint8_t>(r * 255),
		static_cast<uint8_t>(g * 255),
		static_cast<uint8_t>(b * 255),
		static_cast<uint8_t>((start.a + (end.a - start.a) * t))
	);
}

UIColor ColorUtil::getOceanWaveColor(float time, float phase) {
	float wave1 = sinf(time * 2.0f + phase) * 0.3f + 0.7f;
	float wave2 = sinf(time * 3.2f + phase + 1.57f) * 0.2f + 0.8f;
	float wave3 = sinf(time * 1.8f + phase + 3.14f) * 0.15f + 0.85f;

	float h = 200.0f + sinf(time * 0.5f + phase) * 30.0f;
	float s = 0.6f + wave1 * 0.4f;
	float v = 0.7f + wave2 * 0.3f;

	return UIColor::FromHSV(h, s, v);
}

UIColor ColorUtil::getAquaGradient(float progress, float alpha) {
	progress = std::clamp(progress, 0.0f, 1.0f);

	const std::vector<UIColor> keyColors = {
		UIColor(15, 82, 186, 255),
		UIColor(0, 150, 255, 255),
		UIColor(64, 224, 255, 255),
		UIColor(173, 216, 230, 255),
		UIColor(240, 248, 255, 255)
	};

	return getMultiGradient(keyColors, progress);
}

UIColor ColorUtil::getCrystalShine(float time, float phase) {
	float shimmer = powf(sinf(time * 4.0f + phase), 4.0f);
	float base = 0.4f + shimmer * 0.6f;

	return UIColor(
		static_cast<uint8_t>(150 * base + shimmer * 105),
		static_cast<uint8_t>(200 * base + shimmer * 55),
		static_cast<uint8_t>(255 * base),
		255
	);
}

UIColor ColorUtil::getLiquidFlow(float time, float phase) {
	float flow1 = sinf(time * 2.5f + phase) * 0.4f + 0.6f;
	float flow2 = sinf(time * 3.7f + phase + 2.0f) * 0.3f + 0.7f;
	float turbulence = sinf(time * 8.0f + phase) * 0.1f + 0.9f;

	float h = 190.0f + flow1 * 40.0f;
	float s = 0.7f + flow2 * 0.3f;
	float v = 0.8f * turbulence;

	return UIColor::FromHSV(h, s, v);
}

UIColor ColorUtil::getPlasmaEffect(float time, float phase) {
	float x = sinf(time * 3.0f + phase);
	float y = sinf(time * 2.0f + phase + 1.57f);
	float z = sinf(time * 4.0f + phase + 3.14f);

	float r = (sinf(x + y) + sinf(y + z) + sinf(z + x)) / 3.0f;

	float hue = fmodf((r + 1.0f) * 180.0f + 180.0f, 360.0f);
	return UIColor::FromHSV(hue, 0.8f, 0.9f);
}

UIColor ColorUtil::getSweepGradient(float progress, float baseHue) {
	float hue = fmodf(baseHue + progress * 120.0f, 360.0f);
	float saturation = 0.7f + sinf(progress * 12.56f) * 0.3f;
	float brightness = 0.8f + sinf(progress * 18.84f) * 0.2f;

	return UIColor::FromHSV(hue, saturation, brightness);
}

UIColor ColorUtil::getEnergyFlow(float time, float phase) {
	float energy = sinf(time * 5.0f + phase) * 0.5f + 0.5f;
	energy = powf(energy, 0.5f);

	float h = 220.0f - energy * 20.0f;
	float s = 1.0f - energy * 0.3f;
	float v = 0.6f + energy * 0.4f;

	return UIColor::FromHSV(h, s, v);
}

UIColor ColorUtil::getNeonGlow(float time, float hue, float intensity) {
	float glow = (sinf(time * 7.0f) + 1.0f) * 0.5f;
	glow = powf(glow, 3.0f) * intensity;

	float s = 0.8f + glow * 0.2f;
	float v = 0.7f + glow * 0.3f;

	return UIColor::FromHSV(hue, s, v);
}

UIColor ColorUtil::getMultiGradient(const std::vector<UIColor>& colors, float progress) {
	if (colors.empty()) return UIColor();
	if (colors.size() == 1) return colors[0];

	progress = std::clamp(progress, 0.0f, 1.0f);
	float scaledProgress = progress * (colors.size() - 1);
	int index = static_cast<int>(scaledProgress);
	float localProgress = scaledProgress - index;

	if (index >= static_cast<int>(colors.size()) - 1) {
		return colors.back();
	}

	return lerp(colors[index], colors[index + 1], localProgress);
}

UIColor ColorUtil::getAquaThemeColor(int variant, float alpha) {
	const std::vector<UIColor> aquaColors = {
		UIColor(64, 158, 255),
		UIColor(0, 191, 255),
		UIColor(30, 144, 255),
		UIColor(135, 206, 250),
		UIColor(173, 216, 230),
		UIColor(224, 255, 255),
		UIColor(100, 149, 237),
		UIColor(70, 130, 180),
		UIColor(176, 196, 222),
		UIColor(95, 158, 160)
	};

	int index = std::clamp(variant, 0, static_cast<int>(aquaColors.size()) - 1);
	UIColor color = aquaColors[index];
	color.a = static_cast<uint8_t>(alpha * 255);
	return color;
}

ID2D1LinearGradientBrush* ColorUtil::createLinearGradientBrush(
	ID2D1RenderTarget* renderTarget,
	const std::vector<GradientStop>& stops,
	const D2D1_POINT_2F& startPoint,
	const D2D1_POINT_2F& endPoint
) {
	if (!renderTarget || stops.empty()) return nullptr;

	std::vector<D2D1_GRADIENT_STOP> d2dStops;
	d2dStops.reserve(stops.size());

	for (const auto& stop : stops) {
		D2D1_GRADIENT_STOP d2dStop;
		d2dStop.position = std::clamp(stop.position, 0.0f, 1.0f);
		d2dStop.color = stop.color.toD2D1Color();
		d2dStops.push_back(d2dStop);
	}

	ID2D1GradientStopCollection* stopCollection = nullptr;
	HRESULT hr = renderTarget->CreateGradientStopCollection(
		d2dStops.data(),
		static_cast<UINT32>(d2dStops.size()),
		&stopCollection
	);

	if (FAILED(hr)) return nullptr;

	ID2D1LinearGradientBrush* brush = nullptr;
	hr = renderTarget->CreateLinearGradientBrush(
		D2D1::LinearGradientBrushProperties(startPoint, endPoint),
		stopCollection,
		&brush
	);

	stopCollection->Release();
	return SUCCEEDED(hr) ? brush : nullptr;
}

ID2D1RadialGradientBrush* ColorUtil::createRadialGradientBrush(
	ID2D1RenderTarget* renderTarget,
	const std::vector<GradientStop>& stops,
	const D2D1_POINT_2F& center,
	float radiusX,
	float radiusY
) {
	if (!renderTarget || stops.empty()) return nullptr;

	std::vector<D2D1_GRADIENT_STOP> d2dStops;
	d2dStops.reserve(stops.size());

	for (const auto& stop : stops) {
		D2D1_GRADIENT_STOP d2dStop;
		d2dStop.position = std::clamp(stop.position, 0.0f, 1.0f);
		d2dStop.color = stop.color.toD2D1Color();
		d2dStops.push_back(d2dStop);
	}

	ID2D1GradientStopCollection* stopCollection = nullptr;
	HRESULT hr = renderTarget->CreateGradientStopCollection(
		d2dStops.data(),
		static_cast<UINT32>(d2dStops.size()),
		&stopCollection
	);

	if (FAILED(hr)) return nullptr;

	ID2D1RadialGradientBrush* brush = nullptr;
	hr = renderTarget->CreateRadialGradientBrush(
		D2D1::RadialGradientBrushProperties(center, D2D1::Point2F(0, 0), radiusX, radiusY),
		stopCollection,
		&brush
	);

	stopCollection->Release();
	return SUCCEEDED(hr) ? brush : nullptr;
}

std::vector<ColorUtil::GradientStop> ColorUtil::getAquaGradientStops(float alpha) {
	return {
		{0.0f, UIColor(15, 82, 186, static_cast<uint8_t>(alpha * 255))},
		{0.25f, UIColor(0, 150, 255, static_cast<uint8_t>(alpha * 255))},
		{0.5f, UIColor(64, 224, 255, static_cast<uint8_t>(alpha * 255))},
		{0.75f, UIColor(173, 216, 230, static_cast<uint8_t>(alpha * 255))},
		{1.0f, UIColor(240, 248, 255, static_cast<uint8_t>(alpha * 255))}
	};
}

std::vector<ColorUtil::GradientStop> ColorUtil::getOceanGradientStops(float time, float alpha) {
	float wave = sinf(time * 2.0f) * 0.3f + 0.7f;
	uint8_t a = static_cast<uint8_t>(alpha * 255);

	return {
		{0.0f, UIColor(static_cast<uint8_t>(25 * wave), static_cast<uint8_t>(25 * wave), static_cast<uint8_t>(112 * wave), a)},
		{0.3f, UIColor(static_cast<uint8_t>(0 * wave), static_cast<uint8_t>(105 * wave), static_cast<uint8_t>(148 * wave), a)},
		{0.6f, UIColor(static_cast<uint8_t>(64 * wave), static_cast<uint8_t>(224 * wave), static_cast<uint8_t>(255 * wave), a)},
		{1.0f, UIColor(static_cast<uint8_t>(240 * wave), static_cast<uint8_t>(248 * wave), static_cast<uint8_t>(255 * wave), a)}
	};
}

std::vector<ColorUtil::GradientStop> ColorUtil::getSunsetGradientStops(float alpha) {
	uint8_t a = static_cast<uint8_t>(alpha * 255);
	return {
		{0.0f, UIColor(255, 94, 77, a)},
		{0.33f, UIColor(255, 154, 0, a)},
		{0.66f, UIColor(255, 206, 84, a)},
		{1.0f, UIColor(255, 238, 173, a)}
	};
}

std::vector<ColorUtil::GradientStop> ColorUtil::getNeonGradientStops(float time, float alpha) {
	float pulse = (sinf(time * 5.0f) + 1.0f) * 0.5f;
	uint8_t intensity = static_cast<uint8_t>(pulse * 255);
	uint8_t a = static_cast<uint8_t>(alpha * 255);

	return {
		{0.0f, UIColor(intensity, 0, intensity, a)},
		{0.5f, UIColor(0, intensity, intensity, a)},
		{1.0f, UIColor(intensity, intensity, 255, a)}
	};
}

std::string ColorUtil::ColorToHexString(const UIColor& color) {
	char hexBuffer[16];
	snprintf(hexBuffer, sizeof(hexBuffer), "%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
	return std::string(hexBuffer);
}

UIColor ColorUtil::getBreathingColor(const UIColor& color, float time, float speed) {
	float breathe = (sinf(time * speed * 2.0f) + 1.0f) * 0.5f;

	float brightness = 0.3f + breathe * 0.7f;

	return UIColor(
		static_cast<uint8_t>(color.r * brightness),
		static_cast<uint8_t>(color.g * brightness),
		static_cast<uint8_t>(color.b * brightness),
		color.a
	);
}

UIColor ColorUtil::getPulseColor(const UIColor& color, float time, float intensity) {
	float pulse = (sinf(time * 4.0f) + 1.0f) * 0.5f;
	pulse = powf(pulse, 2.0f);

	float effect = 1.0f + pulse * intensity;

	return UIColor(
		static_cast<uint8_t>(std::clamp(color.r * effect, 0.0f, 255.0f)),
		static_cast<uint8_t>(std::clamp(color.g * effect, 0.0f, 255.0f)),
		static_cast<uint8_t>(std::clamp(color.b * effect, 0.0f, 255.0f)),
		color.a
	);
}

UIColor ColorUtil::getGlowPulse(const UIColor& baseColor, float time, float intensity) {
	float pulse = (sinf(time * 3.0f) + 1.0f) * 0.5f;
	pulse = powf(pulse, 1.5f);

	float glow = 1.0f + pulse * intensity * 0.8f;

	float h, s, v;
	ImGui::ColorConvertRGBtoHSV(baseColor.r / 255.0f, baseColor.g / 255.0f, baseColor.b / 255.0f, h, s, v);

	s = std::clamp(s + pulse * intensity * 0.2f, 0.0f, 1.0f);
	v = std::clamp(v * glow, 0.0f, 1.0f);

	float r, g, b;
	ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);

	return UIColor(
		static_cast<uint8_t>(r * 255),
		static_cast<uint8_t>(g * 255),
		static_cast<uint8_t>(b * 255),
		baseColor.a
	);
}
