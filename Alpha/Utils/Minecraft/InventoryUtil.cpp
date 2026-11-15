#include "InventoryUtil.h"
#include "../../sdk/Classes/Item.h"
#include "../../sdk/Classes/LocalPlayer.h"
#include "../../sdk/GameVersion.h"
#include <string>

PlayerInventory* InventoryUtil::getPlayerInventory() {
	LocalPlayer* lp = Game::getLocalPlayer();
	if (!lp) return nullptr;
	return lp->playerInventory;
}

Container* InventoryUtil::getInventory() {
	PlayerInventory* inv = getPlayerInventory();
	if (!inv) return nullptr;
	return inv->container;
}

ItemStack* InventoryUtil::getItem(int32_t slot) {
	Container* inv = getInventory();
	if (!inv) return nullptr;
	return inv->getItem(slot);
}

void InventoryUtil::switchSlot(int32_t slot) {
	PlayerInventory* inv = getPlayerInventory();
	if (!inv) return;
	inv->mSelected = slot;
}

uint8_t InventoryUtil::getSelectedSlot() {
	PlayerInventory* inv = getPlayerInventory();
	if (!inv) return 0;
	return (uint8_t)inv->mSelected;
}

bool InventoryUtil::isValid(ItemStack* item) {
	if (!item) return false;
	if (SdkVersion::is12651()) {
		if (item->mCount == 0) return false;
	} else if (!item->mValid) return false;
	if (!item->mItem.get()) return false;
	return true;
}

bool InventoryUtil::isBlock(ItemStack* item) {
	if (!isValid(item)) return false;
	return item->isBlock();
}

int InventoryUtil::getItemId(ItemStack* stack) {
	if (!stack) return 0;
	Item* item = stack->mItem.get();
	if (!item) return 0;
	return item->mId;
}

int InventoryUtil::getHeldItemId() {
	return getItemId(getItem(getSelectedSlot()));
}

bool InventoryUtil::hasItem(int itemId) {
	for (int i = 0; i < 9; i++) {
		ItemStack* stack = getItem(i);
		if (isValid(stack) && getItemId(stack) == itemId) return true;
	}
	return false;
}

int32_t InventoryUtil::findItemSlotInHotbar(uint16_t itemId) {
	for (int i = 0; i < 9; i++) {
		ItemStack* stack = getItem(i);
		if (isValid(stack) && getItemId(stack) == itemId) return i;
	}
	return -1;
}

int32_t InventoryUtil::findItemSlotInInventory(uint16_t itemId) {
	for (int i = 0; i < 36; i++) {
		ItemStack* stack = getItem(i);
		if (isValid(stack) && getItemId(stack) == itemId) return i;
	}
	return -1;
}

int InventoryUtil::getItemCooldown(ItemStack* item) {
	if (!item) return 250;
	Item* itemPtr = item->mItem.get();
	if (!itemPtr) return 250;

	const std::string& name = itemPtr->mRawNameId.mStr;

	if (name.find("axe") != std::string::npos) {
		if (name.find("wooden") != std::string::npos || name.find("stone") != std::string::npos) return 1250;
		else if (name.find("diamond") != std::string::npos || name.find("netherite") != std::string::npos || name.find("gold") != std::string::npos) return 1000;
		else return 1100;
	}
	else if (name.find("sword") != std::string::npos) return 600;
	else if (name.find("pickaxe") != std::string::npos) return 850;
	else if (name.find("hoe") != std::string::npos) return 500;
	else if (name.find("shovel") != std::string::npos) return 1000;
	else if (itemPtr->mId == 326) return 1500;
	return 250;
}
