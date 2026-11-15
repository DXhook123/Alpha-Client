#pragma once

#include "IEntityComponent.h"
#include <cstddef>
#include <cstdint>
#include <string>

struct SynchedActorDataItem {
	void** vtable;

	[[nodiscard]] uint16_t getId() const {
		using Func = uint16_t(__fastcall*)(const SynchedActorDataItem*);
		return reinterpret_cast<Func>(vtable[1])(this);
	}

	[[nodiscard]] uint8_t getType() const {
		using Func = uint8_t(__fastcall*)(const SynchedActorDataItem*);
		return reinterpret_cast<Func>(vtable[2])(this);
	}

	[[nodiscard]] std::string* getString() {
		if (!vtable || getType() != 4)
			return nullptr;
		return reinterpret_cast<std::string*>(reinterpret_cast<uintptr_t>(this) + 0x10);
	}
};

struct SynchedActorDataComponent : IEntityComponent {
	SynchedActorDataItem** itemsBegin;
	SynchedActorDataItem** itemsEnd;
	SynchedActorDataItem** itemsCapacity;
	std::byte unknown[0x30];

	[[nodiscard]] SynchedActorDataItem* getItem(uint16_t id) const {
		if (!itemsBegin || !itemsEnd || itemsEnd < itemsBegin)
			return nullptr;

		const auto count = static_cast<size_t>(itemsEnd - itemsBegin);
		if (id >= count)
			return nullptr;
		return itemsBegin[id];
	}
};

static_assert(sizeof(SynchedActorDataComponent) == 0x48);
