#pragma once
#include "../GameVersion.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "../../Utils/MemoryUtil.h"

using EffectDuration = std::int32_t;

struct MobEffectFactorCalculationData {
	std::byte raw[0x60];
};

static_assert(sizeof(std::optional<EffectDuration>) == 0x8);
static_assert(sizeof(MobEffectFactorCalculationData) == 0x60);

struct MobEffectInstanceLayout {
	std::uint32_t mId;
	EffectDuration mDuration;
	std::optional<EffectDuration> mDurationEasy;
	std::optional<EffectDuration> mDurationNormal;
	std::optional<EffectDuration> mDurationHard;
	std::int32_t mAmplifier;
	bool mDisplayOnScreenTextureAnimation;
	bool mIsCounterPausedThisTick;
	bool mAmbient;
	bool mEffectVisible;
	MobEffectFactorCalculationData mFactorCalculationData;
};

static_assert(offsetof(MobEffectInstanceLayout, mId) == 0x0);
static_assert(offsetof(MobEffectInstanceLayout, mDuration) == 0x4);
static_assert(offsetof(MobEffectInstanceLayout, mDurationEasy) == 0x8);
static_assert(offsetof(MobEffectInstanceLayout, mDurationNormal) == 0x10);
static_assert(offsetof(MobEffectInstanceLayout, mDurationHard) == 0x18);
static_assert(offsetof(MobEffectInstanceLayout, mAmplifier) == 0x20);
static_assert(offsetof(MobEffectInstanceLayout, mDisplayOnScreenTextureAnimation) == 0x24);
static_assert(offsetof(MobEffectInstanceLayout, mIsCounterPausedThisTick) == 0x25);
static_assert(offsetof(MobEffectInstanceLayout, mAmbient) == 0x26);
static_assert(offsetof(MobEffectInstanceLayout, mEffectVisible) == 0x27);
static_assert(offsetof(MobEffectInstanceLayout, mFactorCalculationData) == 0x28);
static_assert(sizeof(MobEffectInstanceLayout) == 0x88);

struct MobEffectInstanceLayout12651 {
	std::uint32_t mId;
	EffectDuration mDuration;
	std::optional<EffectDuration> mDurationEasy;
	std::optional<EffectDuration> mDurationNormal;
	std::optional<EffectDuration> mDurationHard;
	std::int32_t mAmplifier;
	bool mDisplayOnScreenTextureAnimation;
	bool mIsCounterPausedThisTick;
	bool mAmbient;
	bool mEffectVisible;
	bool mIsLeveled;
	alignas(8) MobEffectFactorCalculationData mFactorCalculationData;
};

static_assert(offsetof(MobEffectInstanceLayout12651, mId) == 0x0);
static_assert(offsetof(MobEffectInstanceLayout12651, mDuration) == 0x4);
static_assert(offsetof(MobEffectInstanceLayout12651, mAmplifier) == 0x20);
static_assert(offsetof(MobEffectInstanceLayout12651, mIsLeveled) == 0x28);
static_assert(offsetof(MobEffectInstanceLayout12651, mFactorCalculationData) == 0x30);
static_assert(sizeof(MobEffectInstanceLayout12651) == 0x90);

class MobEffectInstance {
public:
	static std::size_t nativeSize() {
		return SdkVersion::is12651() ? sizeof(MobEffectInstanceLayout12651) : sizeof(MobEffectInstanceLayout);
	}

	CLASS_MEMBER(std::uint32_t, id, 0x0);
	CLASS_MEMBER(EffectDuration, duration, 0x4);
	CLASS_MEMBER(std::optional<EffectDuration>, durationEasy, 0x8);
	CLASS_MEMBER(std::optional<EffectDuration>, durationNormal, 0x10);
	CLASS_MEMBER(std::optional<EffectDuration>, durationHard, 0x18);
	CLASS_MEMBER(std::int32_t, amplifier, 0x20);
	CLASS_MEMBER(bool, displayOnScreenTextureAnimation, 0x24);
	CLASS_MEMBER(bool, isCounterPausedThisTick, 0x25);
	CLASS_MEMBER(bool, ambient, 0x26);
	CLASS_MEMBER(bool, effectVisible, 0x27);
	CLASS_MEMBER(MobEffectFactorCalculationData, factorCalculationData, 0x30);

	std::uint32_t getId() const {
		return *reinterpret_cast<const std::uint32_t*>(reinterpret_cast<std::uintptr_t>(this) + 0x0);
	}
	EffectDuration getDuration() const {
		return *reinterpret_cast<const EffectDuration*>(reinterpret_cast<std::uintptr_t>(this) + 0x4);
	}
	std::int32_t getAmplifier() const {
		return *reinterpret_cast<const std::int32_t*>(reinterpret_cast<std::uintptr_t>(this) + 0x20);
	}

	unsigned int getTimeLeftSeconds() const {
		const EffectDuration ticks = getDuration();
		return ticks > 0 ? static_cast<unsigned int>(ticks / 20) : 0u;
	}

	std::string getTimeLeftStr() const {
		const unsigned int seconds = getTimeLeftSeconds();
		std::string result = std::to_string(seconds / 60);
		if (seconds % 60 < 10) result += ":0";
		else result += ":";
		result += std::to_string(seconds % 60);
		return result;
	}
};
