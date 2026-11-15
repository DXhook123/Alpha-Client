#pragma once
#include "../../Utils/MemoryUtil.h"
#include "../../Utils/Maths.h"
#include "../Signatures.h"
#include <atomic>

class Item;
class Block;
class CompoundTag;

template <typename T>
class SharedCounter {
public:
	T* ptr;
	std::atomic_int share_count;
	std::atomic_int weak_count;
};

template <typename T>
class WeakPtr {
	SharedCounter<T>* counter;
public:
	T* get() const { return counter ? counter->ptr : nullptr; }
	T* operator->() const { return get(); }
	operator bool() const { return counter && counter->ptr; }
};

class ItemStackBase {
public:
	CLASS_MEMBER(uintptr_t**, vftable, 0x0);
	CLASS_MEMBER(WeakPtr<Item>, mItem, 0x8);
	CLASS_MEMBER(CompoundTag*, mUserData, 0x10);
	CLASS_MEMBER(const Block*, mBlock, 0x18);
	CLASS_MEMBER(short, mAuxValue, 0x20);
	CLASS_MEMBER(unsigned char, mCount, 0x22);
	CLASS_MEMBER(bool, mValid, 0x23);
	CLASS_MEMBER(bool, mShowPickUp, 0x24);
	CLASS_MEMBER(bool, mWasPickedUp, 0x25);

	Item* getItem() {
		if (this && mItem)
			return mItem.get();
		return nullptr;
	}

	short getDamageValue() {
		return mAuxValue;
	}

	bool isBlock() {
		return this && mBlock != nullptr;
	}
};
