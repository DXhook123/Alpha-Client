#pragma once
#include "IEntityComponent.h"
#include "../GameVersion.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

class AttributeInstance {
public:
	std::byte pad0[0x68];

	float mDefaultMinValue;
	float mDefaultMaxValue;
	float mDefaultValue;

	float mCurrentMinValue;
	float mCurrentMaxValue;
	float mCurrentValue;
};
static_assert(offsetof(AttributeInstance, mDefaultMinValue) == 0x68);
static_assert(offsetof(AttributeInstance, mCurrentMinValue) == 0x74);
static_assert(offsetof(AttributeInstance, mCurrentValue) == 0x7C);
static_assert(sizeof(AttributeInstance) == 0x80);

enum AttributeId : unsigned int {
	HealthId = 7,
	MovementSpeedId = 5,
	AbsorptionId = 14,
};

struct BaseAttributeMap {
	uint32_t* keys;
	uint32_t* keysEnd;
	uint32_t* keysCap;
	AttributeInstance* values;
	AttributeInstance* valuesEnd;
	AttributeInstance* valuesCap;
	size_t instanceCount() const {
		const auto begin = reinterpret_cast<uintptr_t>(values);
		const auto end = reinterpret_cast<uintptr_t>(valuesEnd);
		const auto kb = reinterpret_cast<uintptr_t>(keys);
		const auto ke = reinterpret_cast<uintptr_t>(keysEnd);
		const auto stride = SdkVersion::current().attributeInstanceSize;
		if (!begin || !kb || end < begin || ke < kb ||
			(end - begin) % stride || (ke - kb) % sizeof(uint32_t)) return 0;
		const size_t n = (ke - kb) / sizeof(uint32_t);
		const size_t vn = (end - begin) / stride;
		return n == vn && n <= 256 ? n : 0;
	}
	AttributeInstance* instanceAt(size_t index) const {
		return reinterpret_cast<AttributeInstance*>(reinterpret_cast<uintptr_t>(values) +
			index * SdkVersion::current().attributeInstanceSize);
	}

	AttributeInstance* getInstanceById(unsigned int id) const {
		const size_t count = instanceCount();
		for (size_t i = 0; i < count; ++i)
			if (keys[i] == id) return instanceAt(i);
		return nullptr;
	}

	static const char* instanceName(const AttributeInstance* it) {
		if (!it) return "";
		uintptr_t addr = reinterpret_cast<uintptr_t>(it) + 0x8;
		const char* attr = reinterpret_cast<const char*>(*(uintptr_t*)addr);
		if (!attr) return "";
		const std::string* nm = reinterpret_cast<const std::string*>(attr);
		if (!nm) return "";
		if (nm->data() == nullptr || nm->size() == 0 || nm->size() > 64)
			return "";
		return nm->c_str();
	}

	AttributeInstance* getHealthInstance() const {
		if (!keys || !values || keysEnd <= keys)
			return nullptr;

		if (auto* health = getInstanceById(HealthId))
			return health;
		if (SdkVersion::is12651()) return getHealthInstanceByValue();

		const auto limit = instanceCount();

		AttributeInstance* best = nullptr;
		AttributeInstance* first = nullptr;
		for (size_t i = 0; i < limit; ++i) {
			AttributeInstance* it = instanceAt(i);
			if (!it)
				continue;
			const char* nm = instanceName(it);
			if (nm && *nm && strstr(nm, "health") != nullptr)
				return it;

			const float dv = it->mDefaultValue;
			const float mx = it->mCurrentMaxValue;
			if (first == nullptr && dv > 0.f && mx > 0.f)
				first = it;
			if (dv <= 0.f || dv > 200.f)
				continue;
			if (!best || dv > best->mDefaultValue)
				best = it;
		}
		return best ? best : first;
	}

	float getHealth() const {
		AttributeInstance* inst = getHealthInstance();
		return inst ? inst->mCurrentValue : 0.f;
	}

	float getMaxHealth() const {
		AttributeInstance* inst = getHealthInstance();
		return inst ? inst->mDefaultValue : 0.f;
	}

	AttributeInstance* getHealthInstanceByValue() const {
		if (!keys || !values || keysEnd <= keys)
			return nullptr;
		const size_t lim = instanceCount();
		for (size_t i = 0; i < lim; ++i) {
			AttributeInstance* it = instanceAt(i);
			const float dv = it->mDefaultValue, mx = it->mCurrentMaxValue, cv = it->mCurrentValue;
			if (dv > 19.9f && dv < 20.1f && mx > 19.9f && mx < 20.1f && cv >= 0.f && cv <= 20.f)
				return it;
		}
		return nullptr;
	}

	static bool looksLikeAbsorption(const AttributeInstance* it) {
		if (!it) return false;
		const float dv = it->mDefaultValue;
		const float mx = it->mCurrentMaxValue;
		const float cv = it->mCurrentValue;
		return dv >= -0.01f && dv <= 0.01f &&
			mx >= 15.5f && mx <= 16.5f &&
			cv >= -0.01f && cv <= mx + 0.01f;
	}

	AttributeInstance* getAbsorptionInstance() const {
		if (!keys || !values || keysEnd <= keys)
			return nullptr;

		if (auto* absorption = getInstanceById(AbsorptionId);
			looksLikeAbsorption(absorption))
			return absorption;

		const size_t lim = instanceCount();
		for (size_t i = 0; i < lim; ++i) {
			AttributeInstance* it = instanceAt(i);
			if (looksLikeAbsorption(it))
				return it;
		}
		return nullptr;
	}

	float getAbsorption() const {
		AttributeInstance* inst = getAbsorptionInstance();
		return inst ? inst->mCurrentValue : 0.f;
	}

	void debugDump(char* buf, size_t buflen) const {
		if (!buf || !buflen) return;
		if (!keys || !values) {
			snprintf(buf, buflen, "map null: keys=%p values=%p", (void*)keys, (void*)values);
			return;
		}
		size_t n = instanceCount();
		size_t i = 0;
		buf[0] = 0;
		int used = 0;
		char* p = buf;
		for (; i < n; ++i) {
			AttributeInstance* it = instanceAt(i);
			float dv = it->mDefaultValue, cv = it->mCurrentValue, mx = it->mCurrentMaxValue;
			bool worth = (i < 4) || (dv > 15.f && dv < 25.f);
			if (!worth) continue;
			const size_t remaining = buflen - static_cast<size_t>(used);
			int w = snprintf(p, remaining, "key=%u def=%.1f cur=%.1f max=%.1f; ",
				keys[i], dv, cv, mx);
			if (w < 0 || static_cast<size_t>(w) >= remaining) break;
			used += w;
			p += w;
		}
	}
};
static_assert(offsetof(BaseAttributeMap, keys) == 0x00);
static_assert(offsetof(BaseAttributeMap, values) == 0x18);
static_assert(offsetof(BaseAttributeMap, keysCap) == 0x10);
static_assert(offsetof(BaseAttributeMap, valuesCap) == 0x28);

struct AttributesComponent : IEntityComponent {
	BaseAttributeMap mAttributeMap;
};
