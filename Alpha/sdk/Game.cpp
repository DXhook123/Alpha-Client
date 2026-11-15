#include "Game.h"
#include "GameVersion.h"
#include "Classes/SetActorMotionPacket.h"
#include "Classes/MovePlayerPacket.h"
#include "Components/MoveInputComponent.h"
#include "Components/Flags/OnGroundFlag.h"
#include "Components/MobHurtTimeComponent.h"

ClientInstance* Game::clientInstance = nullptr;
int Game::viewPerspectiveMode = 0;

void Game::logSDKOffsets() {
}
