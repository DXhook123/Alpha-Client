#pragma once
#include "ItemStackBase.h"
#include <map>

class ItemStack : public ItemStackBase {
public:
	static constexpr size_t SIZE = 0x98;

	std::map<int, int> gatherEnchants() const {
		EnchantSnapshot snapshot{};
		std::map<int, int> result;
		if (!readEnchantSnapshot(this, snapshot)) return result;
		for (size_t i = 0; i < snapshot.count; ++i)
			result[snapshot.entries[i].id] = snapshot.entries[i].level;
		return result;
	}

private:
	struct EnchantEntry { int id; int level; };
	struct EnchantSnapshot { EnchantEntry entries[64]; size_t count; };

	static bool validEnchantAddress(uintptr_t address) {
		return address >= 0x10000 && address <= 0x00007FFFFFFF0000;
	}

	static uintptr_t findEnchantTag(uintptr_t compound, const char* key, size_t keySize, uint8_t type) {
		if (!validEnchantAddress(compound)) return 0;
		const uintptr_t head = *reinterpret_cast<const uintptr_t*>(compound + 0x8);
		const size_t count = *reinterpret_cast<const size_t*>(compound + 0x10);
		if (!validEnchantAddress(head) || count == 0 || count > 4096) return 0;
		uintptr_t node = *reinterpret_cast<const uintptr_t*>(head + 0x8);
		for (size_t depth = 0; depth < 64 && node != head; ++depth) {
			if (!validEnchantAddress(node) || *reinterpret_cast<const uint8_t*>(node + 0x19) != 0)
				return 0;
			const size_t length = *reinterpret_cast<const size_t*>(node + 0x30);
			const size_t capacity = *reinterpret_cast<const size_t*>(node + 0x38);
			if (length > capacity || capacity > 1024 * 1024 || (capacity < 16 && capacity != 15))
				return 0;
			const uintptr_t text = capacity < 16 ? node + 0x20 : *reinterpret_cast<const uintptr_t*>(node + 0x20);
			if (!validEnchantAddress(text)) return 0;
			int comparison = 0;
			for (size_t i = 0; i < keySize && i < length; ++i) {
				comparison = static_cast<unsigned char>(key[i]) - *reinterpret_cast<const uint8_t*>(text + i);
				if (comparison != 0) break;
			}
			if (comparison == 0) comparison = keySize < length ? -1 : (keySize > length ? 1 : 0);
			if (comparison == 0)
				return *reinterpret_cast<const uint8_t*>(node + 0x68) == type ? node + 0x40 : 0;
			node = *reinterpret_cast<const uintptr_t*>(node + (comparison < 0 ? 0 : 0x10));
		}
		return 0;
	}

	static __declspec(noinline) bool readEnchantSnapshot(const ItemStack* stack, EnchantSnapshot& output) {
		output.count = 0;
		__try {
			const uintptr_t address = reinterpret_cast<uintptr_t>(stack);
			if (!validEnchantAddress(address) || !*reinterpret_cast<const uint8_t*>(address + 0x23) ||
				!*reinterpret_cast<const uint8_t*>(address + 0x22)) return false;
			const uintptr_t itemCounter = *reinterpret_cast<const uintptr_t*>(address + 0x8);
			if (!validEnchantAddress(itemCounter) || !*reinterpret_cast<const uintptr_t*>(itemCounter)) return false;
			const uintptr_t compound = *reinterpret_cast<const uintptr_t*>(address + 0x10);
			const uintptr_t list = findEnchantTag(compound, "ench", 4, 9);
			if (!list) return false;
			const uintptr_t begin = *reinterpret_cast<const uintptr_t*>(list + 0x8);
			const uintptr_t end = *reinterpret_cast<const uintptr_t*>(list + 0x10);
			const uintptr_t capacity = *reinterpret_cast<const uintptr_t*>(list + 0x18);
			if (begin == 0) return end == 0 && capacity == 0;
			if (!validEnchantAddress(begin) || end < begin || capacity < end ||
				(end - begin) % sizeof(uintptr_t) != 0 || (capacity - begin) % sizeof(uintptr_t) != 0 ||
				end - begin > sizeof(uintptr_t) * 64 || capacity - begin > sizeof(uintptr_t) * 4096)
				return false;
			if (*reinterpret_cast<const uint8_t*>(list + 0x20) != 10 && begin != end) return false;
			uintptr_t checkedGetId = 0;
			for (uintptr_t current = begin; current < end; current += sizeof(uintptr_t)) {
				const uintptr_t entry = *reinterpret_cast<const uintptr_t*>(current);
				if (!validEnchantAddress(entry)) return false;
				const uintptr_t vtable = *reinterpret_cast<const uintptr_t*>(entry);
				if (!validEnchantAddress(vtable)) return false;
				const uintptr_t getId = *reinterpret_cast<const uintptr_t*>(vtable + 5 * sizeof(uintptr_t));
				if (!validEnchantAddress(getId)) return false;
				if (getId != checkedGetId) {
					MEMORY_BASIC_INFORMATION memory{};
					if (!VirtualQuery(reinterpret_cast<const void*>(getId), &memory, sizeof(memory)) ||
						memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0 ||
						(memory.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) == 0)
						return false;
					checkedGetId = getId;
				}
				using GetTagId = uint8_t(__fastcall*)(const void*);
				if (reinterpret_cast<GetTagId>(getId)(reinterpret_cast<const void*>(entry)) != 10) return false;
				const uintptr_t idTag = findEnchantTag(entry, "id", 2, 2);
				const uintptr_t levelTag = findEnchantTag(entry, "lvl", 3, 2);
				if (!idTag || !levelTag) return false;
				const int id = *reinterpret_cast<const int16_t*>(idTag + 0x8);
				const int level = *reinterpret_cast<const int16_t*>(levelTag + 0x8);
				if (id >= 0 && level > 0) output.entries[output.count++] = { id, level };
			}
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			output.count = 0;
			return false;
		}
	}
};
