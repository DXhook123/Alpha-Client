#include "DebugInfo.h"

#include "Renderer/ImGuiUtils.h"
#include "sdk/Game.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <kiero.h>
#include <string>

namespace {
	constexpr float kScreenPadding = 8.f;
	constexpr float kLineGap = 2.f;

	const UIColor kTextColor(255, 255, 255, 255);

	void drawLine(const std::string& text, float x, float y, float textScale, bool alignRight) {
		if (text.empty())
			return;

		float drawX = x;
		if (alignRight)
			drawX -= ImGuiUtils::getTextWidth(text, textScale);

		ImGuiUtils::drawText(Vec2<float>(drawX, y), text, kTextColor, textScale, true);
	}

	std::string formatPosition(const Vec3<float>& position) {
		if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
			return "? / ? / ?";

		char buffer[128];
		snprintf(buffer, sizeof(buffer), "%.1f / %.1f / %.1f", position.x, position.y, position.z);
		return buffer;
	}

	const char* getDimensionName(int dimension) {
		switch (dimension) {
		case 0:
			return "Overworld";
		case 1:
			return "Nether";
		case 2:
			return "The End";
		default:
			return "Unknown";
		}
	}

	std::string getMemoryInfo() {
		static std::string cached;
		static ULONGLONG lastUpdate = 0;
		const ULONGLONG now = GetTickCount64();

		if (!cached.empty() && now - lastUpdate < 500)
			return cached;

		MEMORYSTATUSEX memoryStatus{};
		memoryStatus.dwLength = sizeof(memoryStatus);
		if (!GlobalMemoryStatusEx(&memoryStatus) || memoryStatus.ullTotalPhys == 0) {
			cached = "Memory Usage: unavailable";
			lastUpdate = now;
			return cached;
		}

		const double usedGB = static_cast<double>(memoryStatus.ullTotalPhys - memoryStatus.ullAvailPhys) /
			(1024.0 * 1024.0 * 1024.0);
		const double totalGB = static_cast<double>(memoryStatus.ullTotalPhys) /
			(1024.0 * 1024.0 * 1024.0);

		char buffer[128];
		snprintf(buffer, sizeof(buffer), "Memory Usage: %.2f GB / %.2f GB", usedGB, totalGB);
		cached = buffer;
		lastUpdate = now;
		return cached;
	}

	std::string getCpuInfo() {
		static std::string cached;
		if (!cached.empty())
			return cached;

		SYSTEM_INFO systemInfo{};
		GetSystemInfo(&systemInfo);

		char buffer[64];
		snprintf(buffer, sizeof(buffer), "CPU: %lu logical processors",
			static_cast<unsigned long>(systemInfo.dwNumberOfProcessors));
		cached = buffer;
		return cached;
	}

	std::string getDisplayInfo() {
		switch (kiero::getRenderType()) {
		case kiero::RenderType::D3D11:
			return "Display: DirectX 11";
		case kiero::RenderType::D3D12:
			return "Display: DirectX 12";
		default:
			return "Display: DirectX";
		}
	}
}

DebugInfo::DebugInfo()
	: Module("DebugInfo", "Show Java-style F3 debug information", Category::CLIENT, VK_F3) {
	registerSetting(new SliderSetting<float>("Text Size", "F3 overlay text scale", &textSize, 0.9f, 0.5f, 1.5f));
}

void DebugInfo::onImGuiRender(ImDrawList* drawlist) {
	if (!drawlist || !ImGui::GetCurrentContext() || !ImGui::GetFont())
		return;

	LocalPlayer* localPlayer = Game::getLocalPlayer();
	if (!localPlayer)
		return;

	ImGuiUtils::setDrawList(drawlist);

	const ImGuiIO& io = ImGui::GetIO();
	const float scale = (std::clamp)(textSize, 0.5f, 1.5f);
	const float lineHeight = (std::max)(ImGuiUtils::getTextHeight(scale), 1.f);
	const float lineStep = lineHeight + kLineGap;

	int dimensionId = -1;
	if (DimensionTypeComponent* dimension = localPlayer->getDimensionTypeComponent())
		dimensionId = dimension->type;

	const Vec3<float> position = localPlayer->getPos();
	const Vec2<float> rotation = localPlayer->rotation ? localPlayer->rotation->mRot : Vec2<float>();

	float leftY = kScreenPadding;
	drawLine("Alpha Client | Minecraft Bedrock", kScreenPadding, leftY, scale, false);
	leftY += lineStep;

	char fpsBuffer[64];
	snprintf(fpsBuffer, sizeof(fpsBuffer), "FPS: %.0f", io.Framerate);
	drawLine(fpsBuffer, kScreenPadding, leftY, scale, false);
	leftY += lineStep;

	char dimensionBuffer[64];
	snprintf(dimensionBuffer, sizeof(dimensionBuffer), "Dimension: %s", getDimensionName(dimensionId));
	drawLine(dimensionBuffer, kScreenPadding, leftY, scale, false);
	leftY += lineStep * 2.f;

	drawLine("XYZ: " + formatPosition(position), kScreenPadding, leftY, scale, false);
	leftY += lineStep;

	if (dimensionId == 1) {
		const Vec3<float> overworldPosition(position.x * 8.f, position.y, position.z * 8.f);
		drawLine("Overworld: " + formatPosition(overworldPosition), kScreenPadding, leftY, scale, false);
		leftY += lineStep;
	}

	char rotationBuffer[96];
	snprintf(rotationBuffer, sizeof(rotationBuffer), "Facing: %.3f / %.3f", rotation.x, rotation.y);
	drawLine(rotationBuffer, kScreenPadding, leftY, scale, false);
	leftY += lineStep;

	const float rightX = io.DisplaySize.x - kScreenPadding;
	float rightY = kScreenPadding;
	drawLine(getMemoryInfo(), rightX, rightY, scale, true);
	rightY += lineStep;
	drawLine(getDisplayInfo(), rightX, rightY, scale, true);
	rightY += lineStep;
	drawLine(getCpuInfo(), rightX, rightY, scale, true);
}
