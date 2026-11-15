#pragma once
#include "../../Utils/MemoryUtil.h"
#include "ItemStack.h"

class Container;
class Inventory;

class PlayerInventory {
public:
	CLASS_MEMBER(int32_t, mSelected, 0x10);
	CLASS_MEMBER(ItemStack, mInfiniteItem, 0x18);
	CLASS_MEMBER(int, mSelectedContainerId, 0xB0);
	CLASS_MEMBER(Container*, container, 0xB8);
	CLASS_MEMBER(std::vector<ItemStack>, mComplexItems, 0xC0);
};
