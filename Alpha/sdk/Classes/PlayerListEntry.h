#pragma once

#include "../Core/mce.h"
#include "SerializedSkinRef.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <string>

class PlayerListEntry {
private:
	char pad[0xA8]{};

public:
	CLASS_MEMBER(std::string, name, 0x18);
	CLASS_MEMBER(SerializedSkinRef, skin, 0x80);
};

static_assert(sizeof(mce::UUID) == 0x10, "UUID layout must remain 16 bytes for the player map");
static_assert(sizeof(PlayerListEntry) == 0xA8, "PlayerListEntry layout must remain 0xA8 bytes");

struct PlayerListUUIDHash {
	std::size_t operator()(const mce::UUID& value) const noexcept {
		const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
		std::size_t hash = static_cast<std::size_t>(1469598103934665603ULL);
		for (size_t i = 0; i < sizeof(mce::UUID); ++i) {
			hash ^= bytes[i];
			hash *= static_cast<std::size_t>(1099511628211ULL);
		}
		return hash;
	}
};

struct PlayerListUUIDEqual {
	bool operator()(const mce::UUID& lhs, const mce::UUID& rhs) const noexcept {
		return std::memcmp(&lhs, &rhs, sizeof(mce::UUID)) == 0;
	}
};

using PlayerListMap = std::unordered_map<mce::UUID, PlayerListEntry, PlayerListUUIDHash, PlayerListUUIDEqual>;
