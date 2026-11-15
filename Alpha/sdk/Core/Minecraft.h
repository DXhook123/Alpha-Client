#pragma once
#include "../../Utils/MemoryUtil.h"
#include "../GameVersion.h"

#include <cstddef>
#include <cstdint>
#include <functional>

class PacketSender;
class GameSession;

class Timer {
public:
	float mTicksPerSecond;
	std::int32_t mTicks;
	float mAlpha;
	float mTimeScale;
	float mPassedTime;
	float mFrameStepAlignmentRemainder;
	float mLastTimeSeconds;
	float mLastTimestep;
	float mOverflowTime;
	std::byte pad_24[0x4];
	std::int64_t mLastMs;
	std::int64_t mLastMsSysTime;
	float mAdjustTime;
	float mSteppingTick;
	std::function<std::int64_t(void)> mGetTimeMSCallback;
};

static_assert(offsetof(Timer, mTicksPerSecond) == 0x00, "Timer::mTicksPerSecond offset changed");
static_assert(offsetof(Timer, mGetTimeMSCallback) == 0x40, "Timer::mGetTimeMSCallback offset changed");
static_assert(sizeof(Timer) == 0x80, "Timer size must match Minecraft 1.21.202");

class Minecraft {
public:
	CLASS_MEMBER(GameSession*, gameSession, SdkVersion::current().gameSession);
	CLASS_MEMBER(Timer*, simTimer, SdkVersion::current().simTimer);
	CLASS_MEMBER(Timer*, realTimer, SdkVersion::current().realTimer);
	CLASS_MEMBER(PacketSender*, packetSender, SdkVersion::current().minecraftPacketSender);
};
