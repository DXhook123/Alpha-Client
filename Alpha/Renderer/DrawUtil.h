#pragma once
#include "../Utils/Maths.h"
#include "../Utils/ColorUtil.h"
#include "../Libs/imgui/imgui.h"

namespace DrawUtil {

	struct FrameTransform {
		float viewProjection[16] = {};
		Vec3<float> cameraPos;
		Vec2<float> fov;
		bool useRefDef = false;
		bool valid = false;
		int matrixMode = 0;
	};

	extern FrameTransform frame;

	void onImGuiWorldRender();

	bool worldToScreen(const Vec3<float>& worldPos, Vec2<float>& outScreen);

	void drawBox3dFilled(const AABB& aabb, const UIColor& fillColor, const UIColor& lineColor, float lineWidth = 1.5f);

	void drawLine3d(const Vec3<float>& start, const Vec3<float>& end, const UIColor& color, float thickness = 1.5f);
	void drawPolygon3dFilled(const Vec3<float>* vertices, int count, const UIColor& color);
}
