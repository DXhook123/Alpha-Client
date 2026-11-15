#pragma once
#include <cstdint>

class ActorRuntimeID {
public:
	uint64_t id = 0;

	ActorRuntimeID() = default;
	explicit ActorRuntimeID(uint64_t _id) : id(_id) {}

	bool operator==(const ActorRuntimeID& rhs) const { return id == rhs.id; }
	bool operator!=(const ActorRuntimeID& rhs) const { return id != rhs.id; }
};
