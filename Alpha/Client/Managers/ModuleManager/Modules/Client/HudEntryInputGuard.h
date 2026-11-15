#pragma once
#include <algorithm>

struct HudEntryInputGuard {
	double until = -1.0;
	float x = 0.f, y = 0.f, radiusSquared = 0.f;
	void reset() { until = -1.0; }
	void arm(double now, float px, float py, float duration, float radius) {
		until = now + (std::max)(0.f, duration);
		x = px; y = py;
		radius = (std::max)(1.f, radius);
		radiusSquared = radius * radius;
	}
	bool blocks(double now, float px, float py, bool pressed) const {
		if (!pressed || now > until) return false;
		const float dx = px-x, dy = py-y;
		return dx*dx + dy*dy <= radiusSquared;
	}
};
