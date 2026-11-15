#pragma once
#include "../Classes/ActorFlags.h"
#include <bitset>
#include <cstdint>
#include <optional>

struct ActorDataFlagComponent {
	std::bitset<0x77> mFlags;

private:
	int nativeBitCount() const {
		return SdkVersion::is12651() ? 131 : 119;
	}

	bool testNativeBit(int index) const {
		if (index < 0 || index >= nativeBitCount()) return false;
		const auto* words = reinterpret_cast<const std::uint64_t*>(this);
		return (words[index / 64] & (std::uint64_t{1} << (index % 64))) != 0;
	}

	void setNativeBit(int index, bool value) {
		if (index < 0 || index >= nativeBitCount()) return;
		auto* words = reinterpret_cast<std::uint64_t*>(this);
		const auto mask = std::uint64_t{1} << (index % 64);
		if (value) words[index / 64] |= mask;
		else words[index / 64] &= ~mask;
	}

public:

	uint64_t getFlags(int index) const {
		uint64_t result = 0;
		for (int i = 0; i < 64; ++i) {
			if (testNativeBit(index + i)) {
				result |= (1ULL << i);
			}
		}
		return result;
	}

	bool getStatusFlag(::ActorFlags flag) const {
		return testNativeBit(nativeActorFlagIndex(flag));
	}

	std::optional<std::bitset<119>> setFlags(uint64_t newFlags, int index) {
		for (int i = 0; i < 64; ++i) {
			setNativeBit(index + i, (newFlags & (1ULL << i)) != 0);
		}
		std::bitset<119> snapshot;
		for (int bit = 0; bit < 119; ++bit) snapshot.set(bit, testNativeBit(bit));
		return snapshot;
	}

	void setStatusFlag(::ActorFlags flag, bool value) {
		setNativeBit(nativeActorFlagIndex(flag), value);
	}
};

static_assert(sizeof(ActorDataFlagComponent) == 0x10);
