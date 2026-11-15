#pragma once

#include "IEntityComponent.h"
#include "../Classes/MobEffectInstance.h"

struct MobEffectsComponent : IEntityComponent {
	MobEffectInstance* mEffects;
	MobEffectInstance* mEffectsEnd;
	MobEffectInstance* mEffectsCapacity;
};

static_assert(sizeof(MobEffectsComponent) == 0x18);
