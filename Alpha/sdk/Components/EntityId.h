#pragma once
#include <entt/entt.hpp>
#include "IEntityComponent.h"
#include "ActorHeadRotationComponent.h"
#include "ActorTypeComponent.h"
#include "RuntimeIDComponent.h"
#include "DimensionTypeComponent.h"
#include "RenderPositionComponent.h"
#include "FallDistanceComponent.h"
#include "ActorWalkAnimationComponent.h"
#include "InsideBlockComponent.h"
#include "OnFireComponent.h"
#include "MobBodyRotationComponent.h"
#include "ActorEquipmentComponent.h"
#include "ActorDataFlagComponent.h"
#include "ActorUniqueIDComponent.h"
#include "SynchedActorDataComponent.h"
#include "MobEffectsComponent.h"
#include "AttributesComponent.h"
#include "Flags/MovementStateFlags.h"
#include <concepts>
#include <cstdint>
#include <string>

struct EntityId {
	std::uint32_t rawId;

	[[nodiscard]] constexpr bool operator==(const EntityId& other) const = default;

	[[nodiscard]] constexpr operator std::uint32_t() const {
		return this->rawId;
	}
};

struct EntityIdTraits {
	using value_type = EntityId;

	using entity_type = uint32_t;
	using version_type = uint16_t;

	static constexpr entity_type entity_mask = 0x3FFFF;
	static constexpr version_type version_mask = 0x3FFF;
};

template <typename Type, typename Allocator>
struct entt::storage_type<Type, EntityId, Allocator, void> {
	using type = entt::basic_storage<Type, EntityId, Allocator>;
};

template<>
struct entt::entt_traits<EntityId> : entt::basic_entt_traits<EntityIdTraits> {
	static constexpr std::size_t page_size = ENTT_SPARSE_PAGE;
};

template<std::derived_from<IEntityComponent> Type, typename Entity, typename Void>
struct entt::component_traits<Type, Entity, Void> {
	using element_type = Type;
	using entity_type = Entity;
	static constexpr bool in_place_delete = true;
	static constexpr std::size_t page_size = ENTT_PACKED_PAGE * !std::is_empty_v<Type>;
};

struct EntityRegistry : std::enable_shared_from_this<EntityRegistry> {
	std::string name;
	entt::basic_registry<EntityId> registry;
	uint32_t id;
};

struct EntityContext {
	EntityRegistry& registry;
	entt::basic_registry<EntityId>& enttRegistry;
	EntityId entity;

	template<typename T>
	[[nodiscard]] T* tryGetComponent() {
		return this->enttRegistry.try_get<T>(this->entity);
	}

	template<typename T>
	[[nodiscard]] T* tryGetComponentByHash(entt::id_type hash) {
		auto* pool = this->enttRegistry.storage(hash);
		if (!pool || !pool->contains(this->entity)) return nullptr;
		return reinterpret_cast<T*>(pool->value(this->entity));
	}

	template<typename T>
	[[nodiscard]] const T* tryGetComponent() const {
		return this->enttRegistry.try_get<T>(this->entity);
	}

	template<typename T>
	[[nodiscard]] bool hasComponent() const {
		return this->enttRegistry.all_of<T>(this->entity);
	}

	template<typename T>
	[[nodiscard]] T& getOrAddComponent() {
		return this->enttRegistry.get_or_emplace<T>(this->entity);
	}

	template<typename T>
	void removeComponent() {
		this->enttRegistry.remove<T>(this->entity);
	}
};

namespace entt {

template<>
struct type_hash<MobEffectsComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return 0xE6A1B550u;
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<AttributesComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("AttributesComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<ActorHeadRotationComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("ActorHeadRotationComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<ActorTypeComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("ActorTypeComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<RuntimeIDComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("RuntimeIDComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<RenderPositionComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("RenderPositionComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<FallDistanceComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("FallDistanceComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<ActorWalkAnimationComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("ActorWalkAnimationComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<InsideBlockComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("InsideBlockComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<OnFireComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("OnFireComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<MobBodyRotationComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("MobBodyRotationComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<DimensionTypeComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("DimensionTypeComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<ActorEquipmentComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("ActorEquipmentComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<ActorDataFlagComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("ActorDataFlagComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<ActorUniqueIDComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("ActorUniqueIDComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<SynchedActorDataComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("SynchedActorDataComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<OnGroundFlagComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("OnGroundFlagComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

template<>
struct type_hash<HorizontalCollisionFlagComponent> {
	[[nodiscard]] static constexpr id_type value() noexcept {
		return hashed_string("HorizontalCollisionFlagComponent").value();
	}
	[[nodiscard]] constexpr operator id_type() const noexcept {
		return value();
	}
};

}
