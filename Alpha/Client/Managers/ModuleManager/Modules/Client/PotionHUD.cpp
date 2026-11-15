#include "PotionHUD.h"

#include "Renderer/ImGuiUtils.h"
#include "sdk/Game.h"
#include "sdk/Classes/LocalPlayer.h"
#include "sdk/Components/MobEffectsComponent.h"
#include "Utils/Spdlog.h"
#include "../Settings/BoolSetting.h"
#include "../Settings/SliderSetting.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>
#include <Windows.h>

namespace {
	struct EffectDescriptor {
		unsigned int id;
		const char* name;
		UIColor color;
	};

	const std::array<EffectDescriptor, 35> kEffects = {{
		{ 1, "Speed", UIColor(124, 175, 198) },
		{ 2, "Slowness", UIColor(90, 108, 129) },
		{ 3, "Haste", UIColor(217, 192, 67) },
		{ 4, "Mining Fatigue", UIColor(74, 66, 23) },
		{ 5, "Strength", UIColor(147, 36, 35) },
		{ 6, "Instant Health", UIColor(248, 36, 35) },
		{ 7, "Instant Damage", UIColor(67, 10, 9) },
		{ 8, "Jump Boost", UIColor(34, 255, 76) },
		{ 9, "Nausea", UIColor(85, 29, 74) },
		{ 10, "Regeneration", UIColor(205, 92, 171) },
		{ 11, "Resistance", UIColor(153, 69, 58) },
		{ 12, "Fire Resistance", UIColor(228, 154, 58) },
		{ 13, "Water Breathing", UIColor(46, 82, 153) },
		{ 14, "Invisibility", UIColor(127, 131, 146) },
		{ 15, "Blindness", UIColor(31, 31, 35) },
		{ 16, "Night Vision", UIColor(31, 31, 161) },
		{ 17, "Hunger", UIColor(88, 118, 83) },
		{ 18, "Weakness", UIColor(72, 77, 72) },
		{ 19, "Poison", UIColor(78, 147, 49) },
		{ 20, "Wither", UIColor(53, 42, 39) },
		{ 21, "Health Boost", UIColor(248, 125, 35) },
		{ 22, "Absorption", UIColor(37, 82, 165) },
		{ 23, "Saturation", UIColor(248, 36, 35) },
		{ 24, "Levitation", UIColor(206, 255, 255) },
		{ 25, "Fatal Poison", UIColor(78, 147, 49) },
		{ 26, "Conduit Power", UIColor(29, 194, 209) },
		{ 27, "Slow Falling", UIColor(206, 255, 255) },
		{ 28, "Bad Omen", UIColor(11, 97, 56) },
		{ 29, "Village Hero", UIColor(68, 255, 68) },
		{ 30, "Darkness", UIColor(41, 39, 33) },
		{ 31, "Trial Omen", UIColor(41, 64, 71) },
		{ 32, "Wind Charged", UIColor(189, 235, 255) },
		{ 33, "Weaving", UIColor(207, 191, 255) },
		{ 34, "Oozing", UIColor(143, 241, 92) },
		{ 35, "Infested", UIColor(140, 104, 93) }
	}};

	struct EffectSnapshot {
		unsigned int duration = 0;
		int amplifier = 0;
	};

	struct ActiveEffect {
		unsigned int id = 0;
		EffectSnapshot effect{};
	};

	constexpr size_t kEffectStride = sizeof(MobEffectInstanceLayout);
	constexpr size_t kProbeComponentBytes = 0x100;
	constexpr size_t kProbeEntries = 64;

	bool isLikelyUserPointer(uintptr_t value) {
		return value >= 0x100000000ull && (value >> 48) == 0;
	}

	bool isReadableRange(uintptr_t address, size_t bytes) {
		if (!isLikelyUserPointer(address) || bytes == 0 || address > UINTPTR_MAX - bytes) return false;
		MEMORY_BASIC_INFORMATION info{};
		if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info)) return false;
		if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
		const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
		return address + bytes <= regionEnd;
	}

	bool readEffectAt(uintptr_t address, ActiveEffect* output) {
		if (!output || !isReadableRange(address, 0x24)) return false;
		__try {
			const MobEffectInstance& effect = *reinterpret_cast<const MobEffectInstance*>(address);
			const EffectDuration duration = effect.getDuration();
			const unsigned int id = effect.getId();
			const int amplifier = effect.getAmplifier();
			const bool infinite = SdkVersion::is12651() && duration == -1;
			if (id == 0 || id > 35 || (duration <= 0 && !infinite) || amplifier < 0 || amplifier > 255) return false;
			output->id = id;
			output->effect.duration = infinite ? 0x7FFFFFFFu : static_cast<unsigned int>(duration);
			output->effect.amplifier = amplifier;
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	void appendUnique(std::vector<ActiveEffect>& effects, const ActiveEffect& candidate) {
		for (ActiveEffect& existing : effects) {
			if (existing.id != candidate.id) continue;
			if (candidate.effect.duration > existing.effect.duration) existing = candidate;
			return;
		}
		effects.push_back(candidate);
	}

	bool readPointerAt(uintptr_t address, uintptr_t* result) {
		if (!result || !isReadableRange(address, sizeof(uintptr_t))) return false;
		__try {
			*result = *reinterpret_cast<const uintptr_t*>(address);
			return *result != 0;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	void collectEffects(MobEffectsComponent* component, std::vector<ActiveEffect>& effects) {
		effects.clear();
		if (!component) return;
		const uintptr_t componentAddress = reinterpret_cast<uintptr_t>(component);
		if (SdkVersion::is12651()) {
			uintptr_t first = 0, last = 0, capacity = 0;
			if (!readPointerAt(componentAddress, &first) ||
				!readPointerAt(componentAddress + sizeof(uintptr_t), &last) ||
				!readPointerAt(componentAddress + 2 * sizeof(uintptr_t), &capacity) ||
				!isLikelyUserPointer(first) || last < first || capacity < last) return;
			const size_t stride = MobEffectInstance::nativeSize();
			const uintptr_t bytes = last - first;
			const uintptr_t reservedBytes = capacity - first;
			if (bytes % stride != 0 || reservedBytes % stride != 0 ||
				reservedBytes / stride > 1024) return;
			for (uintptr_t address = first; address < last; address += stride) {
				ActiveEffect found;
				if (readEffectAt(address, &found)) appendUnique(effects, found);
			}
			return;
		}
		for (size_t fieldOffset = 0; fieldOffset < kProbeComponentBytes; fieldOffset += sizeof(uintptr_t)) {
			uintptr_t table = 0;
			if (!readPointerAt(componentAddress + fieldOffset, &table) || !isLikelyUserPointer(table)) continue;

			for (const EffectDescriptor& descriptor : kEffects) {
				ActiveEffect found;
				if (readEffectAt(table + descriptor.id * kEffectStride, &found) && found.id == descriptor.id)
					appendUnique(effects, found);
			}

			for (size_t index = 0; index < kProbeEntries; ++index) {
				ActiveEffect found;
				if (readEffectAt(table + index * kEffectStride, &found)) appendUnique(effects, found);
			}

			for (size_t index = 0; index < kProbeEntries; ++index) {
				uintptr_t instance = 0;
				if (!readPointerAt(table + index * sizeof(uintptr_t), &instance) || !isLikelyUserPointer(instance)) continue;
				ActiveEffect found;
				if (readEffectAt(instance, &found)) appendUnique(effects, found);
			}
		}
	}

	void dumpEffectStorageOnce(MobEffectsComponent* component) {
		static MobEffectsComponent* lastComponent = nullptr;
		static uintptr_t lastWords[8]{};
		static bool hasWords = false;
		static bool missingLogged = false;
		if (!component) {
			if (!missingLogged) {
				LOG_WARN("[PotionHUD] MobEffectsComponent storage was not found (hash=0xE6A1B550)");
				missingLogged = true;
			}
			lastComponent = nullptr;
			return;
		}
		missingLogged = false;
		uintptr_t words[16]{};
		__try {
			const size_t wordCount = SdkVersion::is12651() ? 3 : std::size(words);
			for (size_t index = 0; index < wordCount; ++index)
				words[index] = *(reinterpret_cast<const uintptr_t*>(component) + index);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			LOG_WARN("[PotionHUD] component=%p could not be read safely", component);
			return;
		}
		bool changed = component != lastComponent || !hasWords;
		for (size_t index = 0; index < std::size(lastWords); ++index)
			changed = changed || words[index] != lastWords[index];
		if (!changed) return;
		lastComponent = component;
		for (size_t index = 0; index < std::size(lastWords); ++index) lastWords[index] = words[index];
		hasWords = true;
		LOG_INFO("[PotionHUD] component=%p raw[0..7]=%p,%p,%p,%p,%p,%p,%p,%p",
			component, reinterpret_cast<void*>(words[0]), reinterpret_cast<void*>(words[1]),
			reinterpret_cast<void*>(words[2]), reinterpret_cast<void*>(words[3]),
			reinterpret_cast<void*>(words[4]), reinterpret_cast<void*>(words[5]),
			reinterpret_cast<void*>(words[6]), reinterpret_cast<void*>(words[7]));
	}

	std::string romanLevel(int level) {
		static constexpr const char* roman[] = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X" };
		if (level >= 1 && level <= 10) return roman[level - 1];
		return std::to_string(level);
	}

	std::string durationText(unsigned int ticks) {
		if (ticks >= 0x7FFFFFF0u) return "infinite";
		const unsigned int seconds = ticks / 20;
		char text[24];
		std::snprintf(text, sizeof(text), "%u:%02u", seconds / 60, seconds % 60);
		return text;
	}
}

PotionHUD::PotionHUD() : HudModule("PotionHUD", "Show active potion effects", Category::Hud) {
	initializeSettings(&size);
	registerSetting(new BoolSetting("Background", "Draw a background behind potion effects", &background, true));
	registerSetting(new SliderSetting<float>("Row Spacing", "Space between potion effects", &rowSpacing, 2.f, 0.f, 10.f));
	anchoredRight = true;
	defaultPos = Vec2<float>(100000.f, 80.f);
	position = defaultPos;
}

void PotionHUD::onImGuiRender(ImDrawList* drawlist) {
	(void)drawlist;
	LocalPlayer* player = Game::getLocalPlayer();
	MobEffectsComponent* component = player ? player->getMobEffectsComponent() : nullptr;
	if (player) dumpEffectStorageOnce(component);
	static MobEffectsComponent* cachedComponent = nullptr;
	static int lastProbeFrame = -1000;
	static std::vector<ActiveEffect> activeEffects;
	const int frame = ImGui::GetFrameCount();
	if (component != cachedComponent || frame - lastProbeFrame >= 10) {
		cachedComponent = component;
		lastProbeFrame = frame;
		collectEffects(component, activeEffects);
	}

	struct Row { std::string name; std::string time; UIColor color; };
	std::vector<Row> rows;
	rows.reserve(kEffects.size());
	for (const EffectDescriptor& descriptor : kEffects) {
		for (const ActiveEffect& active : activeEffects) {
			if (active.id != descriptor.id) continue;
			rows.push_back({ std::string(descriptor.name) + " " + romanLevel(active.effect.amplifier + 1),
				durationText(active.effect.duration), descriptor.color });
			break;
		}
	}

	const bool preview = rows.empty() && isEditing();
	if (rows.empty() && !preview) return;
	if (preview) rows.push_back({ "Potion Effects", "0:00", labelColor });

	const float textScale = (std::clamp)(size, 0.5f, 4.f);
	const float textHeight = ImGuiUtils::getTextHeight(textScale);
	const float rowHeight = textHeight + (std::max)(0.f, rowSpacing);
	const float padding = 4.f * textScale;
	const float gap = 8.f * textScale;
	float width = 0.f;
	for (const Row& row : rows)
		width = (std::max)(width, hudTextWidth(row.name, textScale) + gap + hudTextWidth(row.time, textScale));
	width += padding * 2.f;
	const float height = rowHeight * static_cast<float>(rows.size()) + padding * 2.f;

	const Vec2<float> renderPosition = getClampedRenderPosition(width, height);
	const Vec4<float> rect(renderPosition.x - width, renderPosition.y,
		renderPosition.x, renderPosition.y + height);
	if (background)
		ImGuiUtils::fillRectangle(rect, UIColor(8, 9, 12, static_cast<uint8_t>(opacity)), 2.f);

	float y = rect.y + padding;
	for (const Row& row : rows) {
		const float timeWidth = hudTextWidth(row.time, textScale);
		drawHudText(Vec2<float>(rect.x + padding, y), row.name, getHudColor(row.color), textScale);
		drawHudText(Vec2<float>(rect.z - padding - timeWidth, y), row.time,
			getHudColor(UIColor(255, 255, 255, 255)), textScale);
		y += rowHeight;
	}

	drawEditorRect(rect);
}
