#ifndef VIRASA_ECS_WORLD_H
#define VIRASA_ECS_WORLD_H

#include "virasa/ecs/Types.h"
#include "virasa/ecs/Components.h"
#include "virasa/math/Transform.h"

#include <cstdint>
#include <vector>

namespace virasa::ecs
{

/**
 * @brief Owns the full state of a single entity-component-system instance.
 *
 * World manages a generational entity table, a parent/child hierarchy,
 * and three independent sparse-set component storages for Transform,
 * MeshComponent, and VisualComponent. World is final and cannot be
 * subclassed. It is default-constructible, movable, but not copyable.
 * No Initialize call is required; a default-constructed World is
 * immediately ready for use.
 */
class World final
{
public:
	World() = default;
	~World() = default;

	World(const World&) = delete;
	World& operator=(const World&) = delete;

	World(World&& other) noexcept;
	World& operator=(World&& other) noexcept;

	/**
	 * @brief Allocates a new live entity and returns its handle.
	 * @return A valid Entity handle whose index and generation identify
	 *         the new slot in the entity table.
	 */
	[[nodiscard]] virasa::ecs::Entity CreateEntity();

	/**
	 * @brief Destroys a live entity and every descendant in its hierarchy subtree.
	 * @param entity A valid entity handle previously returned by CreateEntity.
	 */
	void DestroyEntity(virasa::ecs::Entity entity);

	/**
	 * @brief Returns true if the entity handle refers to a currently live entity.
	 * @param entity Any Entity value, including Entity::Invalid().
	 * @return true if the entity's index is in range, the slot is live, and the
	 *         generation matches; false otherwise.
	 */
	[[nodiscard]] bool IsValid(virasa::ecs::Entity entity) const noexcept;

	/**
	 * @brief Reserves internal storage for at least entityCapacity entities.
	 * @param entityCapacity Minimum number of entities to reserve capacity for.
	 */
	void Reserve(uint32_t entityCapacity);

	/**
	 * @brief Returns the number of currently live entities.
	 * @return Count of entities for which IsValid returns true.
	 */
	[[nodiscard]] uint32_t GetEntityCount() const noexcept;

	/**
	 * @brief Sets the parent of child to parent in the hierarchy.
	 * @param child The entity whose parent is being set.
	 * @param parent The new parent entity, or Entity::Invalid() to make child a root.
	 * @return EcsError::None on success, or an error code describing the failure.
	 */
	virasa::ecs::EcsError SetParent(virasa::ecs::Entity child, virasa::ecs::Entity parent);

	/**
	 * @brief Returns the parent of entity, or Entity::Invalid() if entity is a root.
	 * @param entity A valid entity handle.
	 * @return The parent Entity, or Entity::Invalid() for root entities.
	 */
	[[nodiscard]] virasa::ecs::Entity GetParent(virasa::ecs::Entity entity) const noexcept;

	/**
	 * @brief Returns the children of entity.
	 * @param entity A valid entity handle.
	 * @return Const reference to the vector of child entities.
	 */
	[[nodiscard]] const std::vector<virasa::ecs::Entity>& GetChildren(virasa::ecs::Entity entity) const;

	// --- Transform component ---

	/**
	 * @brief Adds a Transform component to entity.
	 * @param entity A valid entity that does not yet have a Transform component.
	 * @param transform The Transform value to store.
	 */
	void AddTransformComponent(virasa::ecs::Entity entity, const virasa::math::Transform& transform);

	/**
	 * @brief Removes the Transform component from entity.
	 * @param entity A valid entity that currently has a Transform component.
	 */
	void RemoveTransformComponent(virasa::ecs::Entity entity);

	/**
	 * @brief Returns true if entity currently has a Transform component.
	 * @param entity Any entity value.
	 * @return true if the entity is valid and has a Transform component.
	 */
	[[nodiscard]] bool HasTransformComponent(virasa::ecs::Entity entity) const noexcept;

	/**
	 * @brief Returns a const reference to entity's Transform component.
	 * @param entity A valid entity with a Transform component.
	 * @return Const reference to the stored Transform.
	 */
	[[nodiscard]] const virasa::math::Transform& GetTransformComponent(virasa::ecs::Entity entity) const;

	/**
	 * @brief Returns a mutable reference to entity's Transform component.
	 * @param entity A valid entity with a Transform component.
	 * @return Mutable reference to the stored Transform.
	 */
	[[nodiscard]] virasa::math::Transform& GetTransformComponent(virasa::ecs::Entity entity);

	/**
	 * @brief Returns the dense-entities vector for the Transform component storage.
	 * @return Const reference to the vector of entities with Transform components.
	 */
	[[nodiscard]] const std::vector<virasa::ecs::Entity>& GetTransformComponentEntities() const noexcept;

	// --- MeshComponent ---

	/**
	 * @brief Adds a MeshComponent to entity.
	 * @param entity A valid entity that does not yet have a MeshComponent.
	 * @param component The MeshComponent value to store.
	 */
	void AddMeshComponent(virasa::ecs::Entity entity, virasa::ecs::MeshComponent component);

	/**
	 * @brief Removes the MeshComponent from entity.
	 * @param entity A valid entity that currently has a MeshComponent.
	 */
	void RemoveMeshComponent(virasa::ecs::Entity entity);

	/**
	 * @brief Returns true if entity currently has a MeshComponent.
	 * @param entity Any entity value.
	 * @return true if the entity is valid and has a MeshComponent.
	 */
	[[nodiscard]] bool HasMeshComponent(virasa::ecs::Entity entity) const noexcept;

	/**
	 * @brief Returns a const reference to entity's MeshComponent.
	 * @param entity A valid entity with a MeshComponent.
	 * @return Const reference to the stored MeshComponent.
	 */
	[[nodiscard]] const virasa::ecs::MeshComponent& GetMeshComponent(virasa::ecs::Entity entity) const;

	/**
	 * @brief Returns a mutable reference to entity's MeshComponent.
	 * @param entity A valid entity with a MeshComponent.
	 * @return Mutable reference to the stored MeshComponent.
	 */
	[[nodiscard]] virasa::ecs::MeshComponent& GetMeshComponent(virasa::ecs::Entity entity);

	/**
	 * @brief Returns the dense-entities vector for the MeshComponent storage.
	 * @return Const reference to the vector of entities with MeshComponents.
	 */
	[[nodiscard]] const std::vector<virasa::ecs::Entity>& GetMeshComponentEntities() const noexcept;

	// --- VisualComponent ---

	/**
	 * @brief Adds a VisualComponent to entity.
	 * @param entity A valid entity that does not yet have a VisualComponent.
	 * @param component The VisualComponent value to store.
	 */
	void AddVisualComponent(virasa::ecs::Entity entity, virasa::ecs::VisualComponent component);

	/**
	 * @brief Removes the VisualComponent from entity.
	 * @param entity A valid entity that currently has a VisualComponent.
	 */
	void RemoveVisualComponent(virasa::ecs::Entity entity);

	/**
	 * @brief Returns true if entity currently has a VisualComponent.
	 * @param entity Any entity value.
	 * @return true if the entity is valid and has a VisualComponent.
	 */
	[[nodiscard]] bool HasVisualComponent(virasa::ecs::Entity entity) const noexcept;

	/**
	 * @brief Returns a const reference to entity's VisualComponent.
	 * @param entity A valid entity with a VisualComponent.
	 * @return Const reference to the stored VisualComponent.
	 */
	[[nodiscard]] const virasa::ecs::VisualComponent& GetVisualComponent(virasa::ecs::Entity entity) const;

	/**
	 * @brief Returns a mutable reference to entity's VisualComponent.
	 * @param entity A valid entity with a VisualComponent.
	 * @return Mutable reference to the stored VisualComponent.
	 */
	[[nodiscard]] virasa::ecs::VisualComponent& GetVisualComponent(virasa::ecs::Entity entity);

	/**
	 * @brief Returns the dense-entities vector for the VisualComponent storage.
	 * @return Const reference to the vector of entities with VisualComponents.
	 */
	[[nodiscard]] const std::vector<virasa::ecs::Entity>& GetVisualComponentEntities() const noexcept;

private:
	static constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;

	// Per-slot entity table state
	std::vector<uint32_t>                       _generations;   // generation per slot
	std::vector<bool>                           _alive;         // liveness per slot
	std::vector<virasa::ecs::Entity>            _parents;       // parent per slot
	std::vector<std::vector<virasa::ecs::Entity>> _children;    // children per slot

	// Free-list of recyclable slot indices
	std::vector<uint32_t> _freeList;

	// Live entity count
	uint32_t _entityCount = 0;

	// Transform sparse-set
	std::vector<virasa::math::Transform>  _transformValues;
	std::vector<virasa::ecs::Entity>      _transformEntities;
	std::vector<uint32_t>                 _transformSparse;   // indexed by entity.index

	// MeshComponent sparse-set
	std::vector<virasa::ecs::MeshComponent> _meshValues;
	std::vector<virasa::ecs::Entity>        _meshEntities;
	std::vector<uint32_t>                   _meshSparse;

	// VisualComponent sparse-set
	std::vector<virasa::ecs::VisualComponent> _visualValues;
	std::vector<virasa::ecs::Entity>          _visualEntities;
	std::vector<uint32_t>                     _visualSparse;

	// Internal helpers
	void DestroyEntityInternal(virasa::ecs::Entity entity, bool detachFromParent);
	void RemoveTransformComponentInternal(virasa::ecs::Entity entity);
	void RemoveMeshComponentInternal(virasa::ecs::Entity entity);
	void RemoveVisualComponentInternal(virasa::ecs::Entity entity);

	void EnsureSparseCapacity(std::vector<uint32_t>& sparse, uint32_t index);
};

} // namespace virasa::ecs

#endif // VIRASA_ECS_WORLD_H
