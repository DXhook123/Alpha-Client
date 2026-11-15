#pragma once
#include "../../Utils/MemoryUtil.h"
#include "../Core/HashedString.h"
#include <string>

class ItemStackBase;
class Item {
public:
	CLASS_MEMBER(std::string, mTextureAtlasFile, 0x10);
	CLASS_MEMBER(int, mFrameCount, 0x30);
	CLASS_MEMBER(std::string, mIconName, 0x68);
	CLASS_MEMBER(std::string, mAtlasName, 0x88);
	CLASS_MEMBER(unsigned char, mMaxStackSize, 0xA8);
	CLASS_MEMBER(short, mId, 0xAA);
	CLASS_MEMBER(std::string, mDescriptionId, 0xB0);
	CLASS_MEMBER(HashedString, mRawNameId, 0xD0);
	CLASS_MEMBER(std::string, mNamespace, 0x100);
	CLASS_MEMBER(HashedString, mFullName, 0x120);
	CLASS_MEMBER(short, mMaxDamage, 0x150);
	CLASS_MEMBER(int, mMaxUseDuration, 0x154);
};
