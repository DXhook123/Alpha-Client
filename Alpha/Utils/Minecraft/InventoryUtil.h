#pragma once
#include "../../sdk/Classes/PlayerInventory.h"
#include "../../sdk/Classes/Container.h"
#include "../../sdk/Classes/ItemStack.h"
#include "../../sdk/Classes/ItemStackBase.h"
#include "../../sdk/Classes/Item.h"
#include "../../sdk/Game.h"

class InventoryUtil {
public:
	static PlayerInventory* getPlayerInventory();
	static Container* getInventory();
	static ItemStack* getItem(int32_t slot);

	static void switchSlot(int32_t slot);
	static uint8_t getSelectedSlot();

	static bool isValid(ItemStack* item);
	static bool isBlock(ItemStack* item);
	static int getItemId(ItemStack* stack);
	static int getHeldItemId();
	static bool hasItem(int itemId);
	static int32_t findItemSlotInHotbar(uint16_t itemId);
	static int32_t findItemSlotInInventory(uint16_t itemId);

	static int getItemCooldown(ItemStack* item);

};
