#include "TabList.h"

#include "Renderer/ImGuiUtils.h"
#include "sdk/Classes/Level.h"
#include "sdk/Classes/LocalPlayer.h"
#include "sdk/Game.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace {
	constexpr size_t kRowsPerColumn = 16;
	constexpr float kScreenPadding = 8.f;
	constexpr float kPanelTop = 20.f;
	constexpr float kPanelPadding = 8.f;
	constexpr float kRowBoxPaddingX = 3.f;
	constexpr float kRowBoxPaddingY = 2.f;
	constexpr uint32_t kPlayerHeadTextureSize = 8;
}

TabList::TabList()
	: Module("TabList", "Show the player list while holding Tab", Category::CLIENT, VK_TAB, 1) {
	registerSetting(new SliderSetting<float>("Text Size", "Player list text scale", &textSize, 0.9f, 0.5f, 1.5f));
	registerSetting(new SliderSetting<float>("Row Spacing", "Vertical spacing between player entries", &rowSpacing, 5.f, 4.f, 12.f));
	registerSetting(new BoolSetting("Background", "Draw a background behind the player list", &background, true));
	registerSetting(new ColorSetting("Text Color", "Player list text color", &textColor, textColor));
	registerSetting(new ColorSetting("Background Color", "Player list background color", &backgroundColor, backgroundColor));
	registerSetting(new ColorSetting("Row Box Color", "Color of each player entry box", &rowBoxColor, rowBoxColor));
}

void TabList::onEnable() {
	std::lock_guard<std::mutex> lock(playersMutex);
	players.clear();
}

void TabList::onDisable() {
	std::lock_guard<std::mutex> lock(playersMutex);
	players.clear();
}

std::string TabList::stripFormatting(const std::string& value) {
	std::string clean;
	clean.reserve(value.size());

	for (size_t i = 0; i < value.size(); ++i) {
		const unsigned char ch = static_cast<unsigned char>(value[i]);
		if (ch == '\r' || ch == '\n') break;

		if (ch == 0xC2 && i + 1 < value.size() &&
			static_cast<unsigned char>(value[i + 1]) == 0xA7) {
			i += 2;
			continue;
		}
		if (ch == 0xA7) {
			if (i + 1 < value.size()) ++i;
			continue;
		}

		if (ch >= 32 && ch != 127)
			clean += static_cast<char>(ch);
	}

	while (!clean.empty() && std::isspace(static_cast<unsigned char>(clean.back())))
		clean.pop_back();
	return clean;
}

std::vector<unsigned char> TabList::makePlayerHeadRgba(const SkinImage& image) {
	if (!image.hasRgbaBytes() || image.width < 64 || image.width % 8 != 0)
		return {};

	const unsigned char* data = image.bytes.data();
	if (!data)
		return {};

	const uint32_t sourceHeadSize = image.width / 8;
	const uint32_t faceX = sourceHeadSize;
	const uint32_t faceY = sourceHeadSize;
	const uint32_t overlayX = sourceHeadSize * 5;
	if (sourceHeadSize == 0 || faceY + sourceHeadSize > image.height ||
		overlayX + sourceHeadSize > image.width)
		return {};

	std::vector<unsigned char> head(kPlayerHeadTextureSize * kPlayerHeadTextureSize * 4);
	const auto pixel = [&](uint32_t x, uint32_t y) {
		return data + (static_cast<size_t>(y) * image.width + x) * 4u;
	};

	for (uint32_t y = 0; y < kPlayerHeadTextureSize; ++y) {
		const uint32_t sourceY = faceY + (y * sourceHeadSize / kPlayerHeadTextureSize);
		for (uint32_t x = 0; x < kPlayerHeadTextureSize; ++x) {
			const uint32_t sourceX = faceX + (x * sourceHeadSize / kPlayerHeadTextureSize);
			const uint32_t hatX = overlayX + (x * sourceHeadSize / kPlayerHeadTextureSize);
			const unsigned char* base = pixel(sourceX, sourceY);
			const unsigned char* overlay = pixel(hatX, sourceY);
			unsigned char* output = head.data() +
				(static_cast<size_t>(y) * kPlayerHeadTextureSize + x) * 4u;

			const uint32_t overlayAlpha = overlay[3];
			const uint32_t inverseAlpha = 255u - overlayAlpha;
			output[0] = static_cast<unsigned char>((overlay[0] * overlayAlpha + base[0] * inverseAlpha) / 255u);
			output[1] = static_cast<unsigned char>((overlay[1] * overlayAlpha + base[1] * inverseAlpha) / 255u);
			output[2] = static_cast<unsigned char>((overlay[2] * overlayAlpha + base[2] * inverseAlpha) / 255u);
			output[3] = 255;
		}
	}

	return head;
}

void TabList::onNormalTick(LocalPlayer* localPlayer) {
	if (!isEnabled())
		return;

	if (!localPlayer || !localPlayer->level) {
		std::lock_guard<std::mutex> lock(playersMutex);
		players.clear();
		return;
	}

	PlayerListMap* playerList = localPlayer->level->getPlayerList();
	if (!playerList) {
		std::lock_guard<std::mutex> lock(playersMutex);
		players.clear();
		return;
	}

	std::vector<PlayerSnapshot> nextPlayers;
	nextPlayers.reserve(playerList->size());

	for (auto& entry : *playerList) {
		std::string name = stripFormatting(entry.second.name);
		if (name.empty())
			continue;

		PlayerSnapshot snapshot;
		snapshot.name = std::move(name);
		const SkinImage* image = entry.second.skin.getSkinImage();
		if (image) {
			snapshot.headRgba = makePlayerHeadRgba(*image);
		}
		nextPlayers.push_back(std::move(snapshot));
	}

	std::sort(nextPlayers.begin(), nextPlayers.end(), [](const PlayerSnapshot& lhs, const PlayerSnapshot& rhs) {
		return lhs.name < rhs.name;
	});
	nextPlayers.erase(std::unique(nextPlayers.begin(), nextPlayers.end(),
		[](const PlayerSnapshot& lhs, const PlayerSnapshot& rhs) {
			return lhs.name == rhs.name;
		}), nextPlayers.end());

	{
		std::lock_guard<std::mutex> lock(playersMutex);
		players = std::move(nextPlayers);
	}
}

void TabList::onImGuiRender(ImDrawList* drawlist) {
	if (!drawlist || !ImGui::GetCurrentContext() || !ImGui::GetFont()) return;

	if (!isEnabled()) return;

	std::vector<PlayerSnapshot> snapshot;
	{
		std::lock_guard<std::mutex> lock(playersMutex);
		snapshot = players;
	}
	if (snapshot.empty()) return;

	ImGuiUtils::setDrawList(drawlist);

	const ImGuiIO& io = ImGui::GetIO();
	const float scale = (std::clamp)(textSize, 0.5f, 1.5f);
	const float textHeight = (std::max)(ImGuiUtils::getTextHeight(scale), 1.f);
	const float panelPadding = kPanelPadding * scale;
	const float headSize = (std::max)(12.f * scale, textHeight);
	const float headGap = 4.f * scale;
	const float textOffset = headSize + headGap;
	const float contentHeight = (std::max)(headSize, textHeight);
	const float rowGap = (std::clamp)(rowSpacing, 4.f, 12.f) * scale;
	const float rowHeight = contentHeight + rowGap;

	const std::string title = "Player List (" + std::to_string(snapshot.size()) + ")";
	float longestNameWidth = 0.f;
	for (const PlayerSnapshot& player : snapshot)
		longestNameWidth = (std::max)(longestNameWidth, ImGuiUtils::getTextWidth(player.name, scale));

	float columnWidth = ImGuiUtils::getTextWidth(title, scale);
	columnWidth = (std::max)(columnWidth, textOffset + longestNameWidth);
	columnWidth += panelPadding * 2.f;

	const size_t naturalColumns = (snapshot.size() + kRowsPerColumn - 1) / kRowsPerColumn;
	const float availableWidth = (std::max)(io.DisplaySize.x - kScreenPadding * 2.f, columnWidth);
	const size_t maxColumns = (std::max)(static_cast<size_t>(1),
		static_cast<size_t>(availableWidth / (std::max)(columnWidth, 1.f)));
	const size_t columns = (std::min)(naturalColumns, maxColumns);
	const size_t rows = (snapshot.size() + columns - 1) / columns;

	const float panelWidth = columnWidth * static_cast<float>(columns);
	const float panelHeight = textHeight + panelPadding + rowHeight * static_cast<float>(rows) + panelPadding;
	const float panelX = (std::max)(kScreenPadding, (io.DisplaySize.x - panelWidth) * 0.5f);
	const float panelY = kPanelTop;

	if (background) {
		ImGuiUtils::fillRoundedRectangle(
			Vec2<float>(panelX, panelY),
			Vec2<float>(panelX + panelWidth, panelY + panelHeight),
			backgroundColor, 3.f * scale);
		ImGuiUtils::drawRoundedRectangle(
			Vec2<float>(panelX, panelY),
			Vec2<float>(panelX + panelWidth, panelY + panelHeight),
			UIColor(backgroundColor.r, backgroundColor.g, backgroundColor.b,
				static_cast<uint8_t>((std::min)(255, static_cast<int>(backgroundColor.a) + 45))),
			1.f, 3.f * scale);
	}

	const float titleWidth = ImGuiUtils::getTextWidth(title, scale);
	ImGuiUtils::drawText(
		Vec2<float>(panelX + (panelWidth - titleWidth) * 0.5f, panelY + panelPadding * 0.5f),
		title, textColor, scale, true);

	for (size_t index = 0; index < snapshot.size(); ++index) {
		const size_t column = index / rows;
		const size_t row = index % rows;
		const float x = panelX + static_cast<float>(column) * columnWidth + panelPadding;
		const float y = panelY + textHeight + panelPadding + static_cast<float>(row) * rowHeight;
		const float entryWidth = textOffset + longestNameWidth;

		drawlist->AddRectFilled(
			ImVec2(x - kRowBoxPaddingX * scale, y - kRowBoxPaddingY * scale),
			ImVec2(x + entryWidth + kRowBoxPaddingX * scale,
				y + contentHeight + kRowBoxPaddingY * scale),
			rowBoxColor.toImColor(), 2.0f * scale);

		const std::vector<unsigned char>& head = snapshot[index].headRgba;
		if (head.size() == kPlayerHeadTextureSize * kPlayerHeadTextureSize * 4) {
			const float pixel = headSize / kPlayerHeadTextureSize;
			for (uint32_t py = 0; py < kPlayerHeadTextureSize; ++py)
				for (uint32_t px = 0; px < kPlayerHeadTextureSize; ++px) {
					const unsigned char* c = head.data() + (py * kPlayerHeadTextureSize + px) * 4;
					drawlist->AddRectFilled(ImVec2(x + px * pixel, y + py * pixel),
						ImVec2(x + (px + 1) * pixel, y + (py + 1) * pixel), IM_COL32(c[0], c[1], c[2], 255));
				}
		}

		ImGuiUtils::drawText(Vec2<float>(x + textOffset, y), snapshot[index].name, textColor, scale, true);
	}
}
