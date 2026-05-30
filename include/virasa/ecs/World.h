#ifndef VIRASA_ECS_WORLD_H
#define VIRASA_ECS_WORLD_H

#include "virasa/ecs/Types.h"
#include "virasa/ecs/Components.h"
#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/TransformSystem.h"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace virasa::ecs
{

/**
 * @brief Owns the full state of a single entity-component-system instance.
 *
 * World manages entity lifecycle (generational indices), a parent/child hierarchy,
 * a NameComponent sparse-set, and a registry of ComponentSystem instances.
 * It is the sole owner of all registered systems and drives world-transform
 * propagation via UpdateTransforms().
 *
 * World is final, default-constructible, movable, and not copyable.
 */
class World final
{
public:
	/**
	 * @brief Default-constructs a World and registers all built-in component systems.
	 *
	 * After construction, GetEntityCount() == 0 and all built-in systems are ready.
	 */
	World();

	~World() = default;

	World(const World&) = delete;
	World& operator=(const World&) = delete;

	World(World&&) noexcept;
	World& operator=(World&&) noexcept;

	/**
	 * @brief Allocates a new live entity with the given name.
	 * @param name Desired name; empty string defaults to "Entity". Collisions are resolved with a dot-suffix.
	 * @return The new Entity handle.
	 */
	[[nodiscard]] virasa::ecs::Entity CreateEntity(std::string_view name);

	/**
	 * @brief Destroys an entity and all of its descendants recursively.
	 * @param entity A live entity previously returned by CreateEntity.
	 */
	void DestroyEntity(virasa::ecs::Entity entity);

	/**
	 * @brief Returns true if the entity handle is currently live in this World.
	 * @param entity Any Entity value, including Entity::Invalid().
	 * @return true if the entity is live.
	 */
	[[nodiscard]] bool IsValid(virasa::ecs::Entity entity) const noexcept;

	/**
	 * @brief Reserves internal storage for at least entityCapacity entities.
	 * @param entityCapacity Desired minimum capacity.
	 */
	void Reserve(uint32_t entityCapacity);

	/**
	 * @brief Returns the number of currently live entities.
	 * @return Live entity count.
	 */
	[[nodiscard]] uint32_t GetEntityCount() const noexcept;

	/**
	 * @brief Sets the parent of child to parent, maintaining the hierarchy.
	 * @param child The entity to reparent.
	 * @param parent The new parent, or Entity::Invalid() to make child a root.
	 * @return EcsError::None on success, or an error code.
	 */
	virasa::ecs::EcsError SetParent(virasa::ecs::Entity child, virasa::ecs::Entity parent);

	/**
	 * @brief Returns the parent of entity, or Entity::Invalid() if it is a root.
	 * @param entity A live entity.
	 * @return The parent Entity.
	 */
	[[nodiscard]] virasa::ecs::Entity GetParent(virasa::ecs::Entity entity) const noexcept;

	/**
	 * @brief Returns the children of entity.
	 * @param entity A live entity.
	 * @return Const reference to the children vector.
	 */
	[[nodiscard]] const std::vector<virasa::ecs::Entity>& GetChildren(virasa::ecs::Entity entity) const;

	/**
	 * @brief Returns the list of root entities (those with no parent).
	 * @return Const reference to the roots vector.
	 */
	[[nodiscard]] const std::vector<virasa::ecs::Entity>& GetRoots() const noexcept;

	/**
	 * @brief Finds an entity by its exact name.
	 * @param name The name to search for.
	 * @return The matching Entity, or Entity::Invalid() if not found.
	 */
	[[nodiscard]] virasa::ecs::Entity FindEntityByName(std::string_view name) const;

	/**
	 * @brief Returns true if entity has a NameComponent.
	 * @param entity Any Entity value.
	 * @return true if the entity has a NameComponent.
	 */
	[[nodiscard]] bool HasNameComponent(virasa::ecs::Entity entity) const noexcept;

	/**
	 * @brief Returns the NameComponent for entity.
	 * @param entity A live entity with a NameComponent.
	 * @return Const reference to the NameComponent.
	 */
	[[nodiscard]] const virasa::ecs::NameComponent& GetNameComponent(virasa::ecs::Entity entity) const;

	/**
	 * @brief Returns the dense entity vector of the NameComponent storage.
	 * @return Const reference to the dense entities vector.
	 */
	[[nodiscard]] const std::vector<virasa::ecs::Entity>& GetNameComponentEntities() const noexcept;

	/**
	 * @brief Registers a ComponentSystem and returns its assigned ComponentId.
	 * @param system Non-null unique_ptr to the system to register.
	 * @return The ComponentId assigned to the system.
	 */
	[[nodiscard]] virasa::ecs::ComponentId RegisterSystem(std::unique_ptr<virasa::ecs::ComponentSystem> system);

	/**
	 * @brief Returns the ComponentId of the system with the given name, or kInvalidComponentId.
	 * @param name The system name to look up.
	 * @return The ComponentId, or kInvalidComponentId if not found.
	 */
	[[nodiscard]] virasa::ecs::ComponentId GetSystemId(std::string_view name) const noexcept;

	/**
	 * @brief Returns a reference to the system at the given id.
	 * @param id A valid ComponentId.
	 * @return Reference to the ComponentSystem.
	 */
	[[nodiscard]] virasa::ecs::ComponentSystem& GetSystem(virasa::ecs::ComponentId id);

	/**
	 * @brief Returns a const reference to the system at the given id.
	 * @param id A valid ComponentId.
	 * @return Const reference to the ComponentSystem.
	 */
	[[nodiscard]] const virasa::ecs::ComponentSystem& GetSystem(virasa::ecs::ComponentId id) const;

	/**
	 * @brief Finds a system by name, returning a pointer or nullptr.
	 * @param name The system name.
	 * @return Pointer to the ComponentSystem, or nullptr.
	 */
	[[nodiscard]] virasa::ecs::ComponentSystem* FindSystem(std::string_view name) noexcept;

	/**
	 * @brief Returns all live entities that have components in all listed systems.
	 * @param components List of ComponentIds to intersect.
	 * @return Vector of matching entities.
	 */
	[[nodiscard]] std::vector<virasa::ecs::Entity> GetEntities(
		std::initializer_list<virasa::ecs::ComponentId> components) const;

	/**
	 * @brief Returns a reference to the built-in TransformSystem.
	 * @return Reference to the TransformSystem.
	 */
	[[nodiscard]] virasa::ecs::TransformSystem& Transforms() noexcept;

	/**
	 * @brief Returns a const reference to the built-in TransformSystem.
	 * @return Const reference to the TransformSystem.
	 */
	[[nodiscard]] const virasa::ecs::TransformSystem& GetTransforms() const noexcept;

	/**
	 * @brief Propagates dirty local transforms to cached world matrices.
	 *
	 * Reads the TransformSystem's dirty set, expands each dirty entity to its
	 * descendants, recomposes world matrices in parent-before-child order, writes
	 * them back via SetWorld, then clears the dirty set.
	 */
	void UpdateTransforms();


private:
	// --- Entity table ---
	struct Slot
	{
		uint32_t generation = 0u;
		bool live = false;
	};

	std::vector<Slot>					_slots;
	std::vector<virasa::ecs::Entity>	_parents;		// indexed by slot index
	std::vector<std::vector<virasa::ecs::Entity>>	_children;	// indexed by slot index
	std::vector<virasa::ecs::Entity>	_roots;
	std::vector<uint32_t>				_freeList;
	uint32_t							_entityCount = 0u;

	// --- NameComponent sparse set ---
	std::vector<virasa::ecs::NameComponent>	_nameComponents;	// dense values
	std::vector<virasa::ecs::Entity>		_nameEntities;		// dense owning entities
	std::vector<uint32_t>					_nameSparse;		// indexed by entity index

	// --- System registry ---
	std::vector<std::unique_ptr<virasa::ecs::ComponentSystem>>	_systems;
	std::vector<std::pair<std::string, virasa::ecs::ComponentId>>	_systemNameToId;

	// Cached typed pointer to the built-in TransformSystem
	virasa::ecs::TransformSystem*	_transformSystem = nullptr;

	// --- Private helpers ---
	void DestroyEntityInternal(virasa::ecs::Entity entity, bool isRoot);
	[[nodiscard]] std::string ResolveUniqueName(std::string_view name) const;
	void AddNameComponent(virasa::ecs::Entity entity, std::string uniqueName);
	void RemoveNameComponent(virasa::ecs::Entity entity);
	void UpdateTransformsRecursive(
		virasa::ecs::Entity entity,
		const virasa::math::Mat4* parentWorld);
};

} // namespace virasa::ecs

#endif // VIRASA_ECS_WORLD_H
