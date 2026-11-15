#pragma once
#include "../../sdk/Classes/LocalPlayer.h"
#include "../../sdk/Classes/Player.h"
#include "../../sdk/Classes/GameMode.h"
#include "../../sdk/Classes/Block.h"
#include "../../sdk/Classes/BlockPos.h"
#include "../../sdk/Game.h"
#include "WorldUtil.h"

namespace PlayerUtil {
	bool canPlaceBlock(const BlockPos& blockPos, bool airPlace = false);
	bool tryPlaceBlock(const BlockPos& blockPos, bool airPlace = false);
	bool tryPlaceBlockSupport(const BlockPos& pos, int proximity = 5, bool airPlace = false);
}
