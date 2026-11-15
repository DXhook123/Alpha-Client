#pragma once
#include "../../Utils/MemoryUtil.h"
#include "../../Utils/Maths.h"
#include "../Signatures.h"
#include "../GameVersion.h"
#include "../Components/EntityId.h"
#include "../Components/StateVectorComponent.h"
#include "../Components/AABBShapeComponent.h"
#include "../Components/ActorRotationComponent.h"
#include "../Components/ActorHeadRotationComponent.h"
#include "../Components/ActorTypeComponent.h"
#include "../Components/DimensionTypeComponent.h"
#include "../Components/RuntimeIDComponent.h"
#include "../Components/RenderPositionComponent.h"
#include "../Components/FallDistanceComponent.h"
#include "../Components/ActorWalkAnimationComponent.h"
#include "../Components/InsideBlockComponent.h"
#include "../Components/OnFireComponent.h"
#include "../Components/MobBodyRotationComponent.h"
#include "../Components/ActorEquipmentComponent.h"
#include "../Components/MobEffectsComponent.h"
#include "../Components/AttributesComponent.h"
#include "SimpleContainer.h"
#include <cstdint>
#include <string>

class Level;
class Dimension;
class Player;
class ItemStack;
class Block;

enum class ActorSwingSource : uint8_t {
	None = 0,
	Build = 1,
	Mine = 2,
	Interact = 3,
	Attack = 4,
	UseItem = 5,
	ThrowItem = 6,
	DropItem = 7,
	Event = 8,
};

class Actor {
public:
	CLASS_MEMBER(std::string, nameTag, 0x2E0);
	CLASS_MEMBER(uint64_t, legacyUniqueID, 0x1C0);
	CLASS_MEMBER(void*, dimension, 0x1C8);
	CLASS_MEMBER(Level*, level, 0x1D8);
	CLASS_MEMBER(StateVectorComponent*, stateVector, 0x218);
	CLASS_MEMBER(AABBShapeComponent*, aabbShape, 0x220);
	CLASS_MEMBER(ActorRotationComponent*, rotation, 0x228);

public:
	EntityContext* getEntityContext() {
		return reinterpret_cast<EntityContext*>(reinterpret_cast<uintptr_t>(this) + 0x8);
	}

	bool isValid() {
		if (!this)
			return false;
		if (!this->getActorTypeComponent())
			return false;
		if (!this->getRuntimeIDComponent())
			return false;
		if (!this->getRenderPositionComponent())
			return false;
		if (!this->stateVector)
			return false;
		if (!this->level)
			return false;
		if (!this->aabbShape)
			return false;
		return true;
	}
	ActorHeadRotationComponent* getHeadRotationComponent() {
		return getEntityContext()->tryGetComponent<ActorHeadRotationComponent>();
	}
	ActorTypeComponent* getActorTypeComponent() {
		return getEntityContext()->tryGetComponent<ActorTypeComponent>();
	}
	RuntimeIDComponent* getRuntimeIDComponent() {
		return getEntityContext()->tryGetComponent<RuntimeIDComponent>();
	}
	DimensionTypeComponent* getDimensionTypeComponent() {
		return getEntityContext()->tryGetComponent<DimensionTypeComponent>();
	}
	RenderPositionComponent* getRenderPositionComponent() {
		return getEntityContext()->tryGetComponent<RenderPositionComponent>();
	}

	FallDistanceComponent* getFallDistanceComponent() {
		return getEntityContext()->tryGetComponent<FallDistanceComponent>();
	}
	ActorWalkAnimationComponent* getWalkAnimationComponent() {
		return getEntityContext()->tryGetComponent<ActorWalkAnimationComponent>();
	}
	InsideBlockComponent* getInsideBlockComponent() {
		return getEntityContext()->tryGetComponent<InsideBlockComponent>();
	}
	OnFireComponent* getOnFireComponent() {
		return getEntityContext()->tryGetComponent<OnFireComponent>();
	}
	MobBodyRotationComponent* getBodyRotationComponent() {
		return getEntityContext()->tryGetComponent<MobBodyRotationComponent>();
	}
	ActorEquipmentComponent* getEquipmentComponent() {
		return getEntityContext()->tryGetComponent<ActorEquipmentComponent>();
	}
	MobEffectsComponent* getMobEffectsComponent() {
		return getEntityContext()->tryGetComponentByHash<MobEffectsComponent>(0xE6A1B550u);
	}
	ActorDataFlagComponent* getDataFlagComponent() {
		if (SdkVersion::is12651())
			return getEntityContext()->tryGetComponentByHash<ActorDataFlagComponent>(
				entt::type_hash<ActorDataFlagComponent>::value());
		return getEntityContext()->tryGetComponent<ActorDataFlagComponent>();
	}
	AttributesComponent* getAttributesComponent() {
		auto* context = getEntityContext();
		if (!context)
			return nullptr;

		if (auto* component = context->tryGetComponent<AttributesComponent>())
			return component;
		if (auto* component = context->tryGetComponentByHash<AttributesComponent>(0xFD3B0613u))
			return component;

		return context->tryGetComponentByHash<AttributesComponent>(0xB6B29444u);
	}
	float getHealth() {
		AttributesComponent* comp = getAttributesComponent();
		if (!comp)
			return 0.f;
		return comp->mAttributeMap.getHealth();
	}
	float getMaxHealth() {
		AttributesComponent* comp = getAttributesComponent();
		if (!comp)
			return 0.f;
		return comp->mAttributeMap.getMaxHealth();
	}
	float getAbsorption() {
		AttributesComponent* comp = getAttributesComponent();
		if (!comp)
			return 0.f;
		return comp->mAttributeMap.getAbsorption();
	}
	ActorUniqueIDComponent* getUniqueIDComponent() {
		return getEntityContext()->tryGetComponent<ActorUniqueIDComponent>();
	}
	SynchedActorDataComponent* getSynchedActorDataComponent() {
		return getEntityContext()->tryGetComponent<SynchedActorDataComponent>();
	}

	ItemStack* getOffhandSlot() {
		ActorEquipmentComponent* ae = getEquipmentComponent();
		if (!ae) return nullptr;
		SimpleContainer* offhand = ae->mOffhandContainer;
		if (!offhand) return nullptr;
		return offhand->getItem(1);
	}

	ItemStack* getCarriedItem() {
		return MemoryUtil::callVirtualFunc<ItemStack*>(77, this);
	}
	void setOffhandSlot(ItemStack* item) {
		MemoryUtil::callVirtualFunc<void, ItemStack*>(78, this, item);
	}
	std::string* getNameTag() {
		using func_t = std::string const& (__fastcall*)(Actor*);
		static func_t func = (func_t)findSig(Sigs::actor::getNameTag);
		if (func)
			return const_cast<std::string*>(&func(this));
		return &nameTag;
	}
	std::string* getScoreTag() {
		constexpr uint16_t scoreDataId = 0x54;
		auto* component = getSynchedActorDataComponent();
		if (!component)
			return nullptr;

		auto* item = component->getItem(scoreDataId);
		return item ? item->getString() : nullptr;
	}
	Vec3<float> getPos() {
		if (!this || !this->stateVector) return Vec3<float>(0, 0, 0);
		return this->stateVector->mPos;
	}
	bool setPos(const Vec3<float>& pos) {
		using func_t = std::int64_t(__fastcall*)(EntityContext*, const Vec3<float>*, bool);
		static func_t func = reinterpret_cast<func_t>(findSig(Sigs::actor::setPos));
		if (!this || !func)
			return false;
		bool clientSide = true;
		if (this->level)
			clientSide = MemoryUtil::callVirtualFunc<bool>(317, this->level);
		func(this->getEntityContext(), &pos, clientSide);
		return true;
	}

	Vec3<float> getPosPrev() {
		if (!this || !this->stateVector) return Vec3<float>(0.f, 0.f, 0.f);
		return this->stateVector->mPosPrev;
	}

	bool onGround() {
		return this && getEntityContext() &&
			getEntityContext()->hasComponent<OnGroundFlagComponent>();
	}

	bool hasHorizontalCollision() {
		return this && getEntityContext() &&
			getEntityContext()->hasComponent<HorizontalCollisionFlagComponent>();
	}

	Vec3<float> getEyePos() {
		return getRenderPositionComponent()->eyePos;
	}
	float getHeadYaw() {
		if (!this) return 0.f;
		return MemoryUtil::callVirtualFunc<float>(14, this, 1.0f, 0.0f);
	}

	float getFallDistance() {
		auto* comp = getFallDistanceComponent();
		return comp ? comp->mValue : 0.f;
	}

	AABB getAABB() {
		auto* comp = this->aabbShape;
		return comp ? comp->mAABB : AABB();
	}

public:

	bool isAlive() {
		return MemoryUtil::callVirtualFunc<bool>(46, this);
	}
	void lerpMotion(const Vec3<float>& delta) {
		MemoryUtil::callVirtualFunc<void, const Vec3<float>&>(22, this, delta);
	}
	void swing(ActorSwingSource source = ActorSwingSource::None) {
		if (SdkVersion::is12651())
			MemoryUtil::callVirtualFunc<bool>(SdkVersion::current().actorSwing, this, source, uint8_t{0});
		else
			MemoryUtil::callVirtualFunc<bool>(SdkVersion::current().actorSwing, this, source);
	}
};
