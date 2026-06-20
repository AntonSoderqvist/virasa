#ifndef VIRASA_ECS_COMPONENTSYSTEM_H
#define VIRASA_ECS_COMPONENTSYSTEM_H

#include "virasa/ecs/Types.h"

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <functional>

namespace virasa::ecs
{

/// @brief Type alias for the runtime handle identifying a ComponentSystem within a World.
using ComponentId = uint32_t;

/// @brief Sentinel ComponentId meaning "no component system".
inline constexpr ComponentId kInvalidComponentId = 0xFFFFFFFFu;

/// @brief Provenance layer describing the role of a component system's storage.
enum class SystemLayer : uint32_t
{
	Core,
	Editor,
	Debug
};

/// @brief Bitmask of SystemLayer values.
using SystemLayerMask = uint32_t;

/**
 * @brief Abstract, type-erased interface for a component storage system.
 *
 * ComponentSystem is the base class a virasa::ecs::World uses to own and manipulate
 * storage for one component type without naming that type. It is not copyable and not
 * movable; instances are owned through std::unique_ptr<ComponentSystem> held by the World.
 * Every method except Layer is pure virtual; ComponentSystem itself stores no data.
 */
class ComponentSystem
{
public:
	ComponentSystem() = default;
	ComponentSystem(const ComponentSystem&) = delete;
	ComponentSystem& operator=(const ComponentSystem&) = delete;
	ComponentSystem(ComponentSystem&&) = delete;
	ComponentSystem& operator=(ComponentSystem&&) = delete;

	/// @brief Virtual destructor so derived systems are properly destroyed through base pointer.
	virtual ~ComponentSystem() noexcept = default;

	/**
	 * @brief Returns the stable human-readable name of the component type.
	 * @return A const char* that outlives this system.
	 */
	[[nodiscard]] virtual const char* Name() const noexcept = 0;

	/**
	 * @brief Returns the runtime ComponentId assigned by the World at registration.
	 * @return The ComponentId for this system.
	 */
	[[nodiscard]] virtual ComponentId Id() const noexcept = 0;

	/**
	 * @brief Sets the runtime ComponentId assigned to this system.
	 *
	 * Called by virasa::ecs::World::RegisterSystem so that Id() returns the
	 * id the World assigned. Not intended to be called by other code.
	 *
	 * @param id The ComponentId to store.
	 */
	virtual void SetId(ComponentId id) = 0;

	/**
	 * @brief Returns the provenance layer for this system.
	 * @return SystemLayer::Core unless overridden by a derived system.
	 */
	[[nodiscard]] virtual virasa::ecs::SystemLayer Layer() const noexcept;

	/**
	 * @brief Reports whether the given entity owns a component in this system.
	 * @param entity The entity to query.
	 * @return true if the entity has a component, false otherwise.
	 */
	[[nodiscard]] virtual bool Has(virasa::ecs::Entity entity) const noexcept = 0;

	/**
	 * @brief Inserts a component for the entity by copying elementSize bytes from value.
	 * @param entity The entity to add the component to.
	 * @param value Pointer to the source bytes (must be at least elementSize bytes).
	 * @return Pointer to the newly stored bytes.
	 */
	virtual void* AddRaw(virasa::ecs::Entity entity, const void* value) = 0;

	/**
	 * @brief Removes the component owned by the given entity.
	 * @param entity The entity whose component to remove.
	 */
	virtual void Remove(virasa::ecs::Entity entity) = 0;

	/**
	 * @brief Returns the number of stored components.
	 * @return Count of entities with a component in this system.
	 */
	[[nodiscard]] virtual size_t Size() const noexcept = 0;

	/**
	 * @brief Returns the dense vector of entities owning components in this system.
	 * @return Reference to the dense entity vector.
	 */
	[[nodiscard]] virtual const std::vector<virasa::ecs::Entity>& Entities() const noexcept = 0;

	/**
	 * @brief Returns a read-only pointer to the entity's stored component bytes.
	 * @param entity The entity to look up.
	 * @return Pointer to the stored bytes.
	 */
	[[nodiscard]] virtual const void* GetRaw(virasa::ecs::Entity entity) const = 0;

	/**
	 * @brief Overwrites the entity's stored component bytes from value.
	 * @param entity The entity whose component to overwrite.
	 * @param value Pointer to the source bytes (must be at least elementSize bytes).
	 */
	virtual void SetRaw(virasa::ecs::Entity entity, const void* value) = 0;

	/**
	 * @brief Invokes fn once per stored component with the owning entity and a read-only pointer.
	 * @param fn Callback receiving (Entity, const void*) for each stored component.
	 */
	virtual void ForEachRaw(const std::function<void(virasa::ecs::Entity, const void*)>& fn) const = 0;

	/**
	 * @brief Returns the vector of entities flagged dirty since the last clear.
	 * @return Reference to the dirty entity vector.
	 */
	[[nodiscard]] virtual const std::vector<virasa::ecs::Entity>& Dirty() const noexcept = 0;

	/**
	 * @brief Removes one entity from the dirty set.
	 * @param entity The entity to clear from the dirty set.
	 */
	virtual void ClearDirty(virasa::ecs::Entity entity) = 0;

	/// @brief Empties the dirty set.
	virtual void ClearAllDirty() = 0;

	/// @brief Resolves any derived state the concrete system maintains.
	virtual void Update() = 0;

	/**
	 * @brief Creates an independent deep copy of this component system.
	 * @return New ComponentSystem instance with copied storage.
	 */
	[[nodiscard]] virtual std::unique_ptr<virasa::ecs::ComponentSystem> Clone() const = 0;
};

/**
 * @brief Concrete ComponentSystem implementing type-erased byte-backed sparse-set storage.
 *
 * SparseComponentSystem stores trivially-copyable component values as raw bytes in a
 * dense buffer, indexed by a sparse vector for O(1) Has/Get/Set. It supports subclassing
 * through three protected virtual hooks: OnAdd, OnRemoveSwap, and OnSet.
 */
class SparseComponentSystem : public virasa::ecs::ComponentSystem
{
public:
	SparseComponentSystem() = delete;
	SparseComponentSystem(const SparseComponentSystem&) = delete;
	SparseComponentSystem& operator=(const SparseComponentSystem&) = delete;
	SparseComponentSystem(SparseComponentSystem&&) = delete;
	SparseComponentSystem& operator=(SparseComponentSystem&&) = delete;

	/**
	 * @brief Constructs the system with a runtime id, a name, and the per-element byte size.
	 * @param id The ComponentId assigned by the World.
	 * @param name A stable human-readable name; the pointer must outlive this system.
	 * @param elementSize The size in bytes of one component value.
	 * @param layer The provenance layer for this system.
	 */
	explicit SparseComponentSystem(
		virasa::ecs::ComponentId id,
		const char* name,
		size_t elementSize,
		virasa::ecs::SystemLayer layer = virasa::ecs::SystemLayer::Core
	);

	virtual ~SparseComponentSystem() noexcept = default;

	/**
	 * @brief Returns the name supplied at construction.
	 * @return The component type name.
	 */
	[[nodiscard]] const char* Name() const noexcept override;

	/**
	 * @brief Returns the ComponentId supplied at construction.
	 * @return The runtime ComponentId.
	 */
	[[nodiscard]] virasa::ecs::ComponentId Id() const noexcept override;

	/**
	 * @brief Stores the ComponentId assigned by the World.
	 * @param id The ComponentId to store.
	 */
	void SetId(virasa::ecs::ComponentId id) noexcept override;

	/**
	 * @brief Returns the provenance layer supplied at construction.
	 * @return The SystemLayer for this system.
	 */
	[[nodiscard]] virasa::ecs::SystemLayer Layer() const noexcept override;

	/**
	 * @brief Reports whether the entity owns a component in this system.
	 * @param entity The entity to query.
	 * @return true if the entity has a component.
	 */
	[[nodiscard]] bool Has(virasa::ecs::Entity entity) const noexcept override;

	/**
	 * @brief Appends a new component for the entity, copied bytewise from value.
	 * @param entity The entity to add the component to (must not already own one).
	 * @param value Pointer to elementSize bytes to copy.
	 * @return Pointer to the newly stored bytes.
	 */
	void* AddRaw(virasa::ecs::Entity entity, const void* value) override;

	/**
	 * @brief Removes the entity's component via swap-and-pop.
	 * @param entity The entity whose component to remove (must own one).
	 */
	void Remove(virasa::ecs::Entity entity) override;

	/**
	 * @brief Returns the number of stored components.
	 * @return Count of dense slots.
	 */
	[[nodiscard]] size_t Size() const noexcept override;

	/**
	 * @brief Returns the dense entity vector.
	 * @return Reference to the dense-entities vector.
	 */
	[[nodiscard]] const std::vector<virasa::ecs::Entity>& Entities() const noexcept override;

	/**
	 * @brief Returns a read-only pointer to the entity's stored component bytes.
	 * @param entity The entity to look up (must own a component).
	 * @return Pointer to the stored bytes.
	 */
	[[nodiscard]] const void* GetRaw(virasa::ecs::Entity entity) const override;

	/**
	 * @brief Overwrites the entity's stored component bytes from value and marks dirty.
	 * @param entity The entity whose component to overwrite (must own one).
	 * @param value Pointer to elementSize bytes to copy.
	 */
	void SetRaw(virasa::ecs::Entity entity, const void* value) override;

	/**
	 * @brief Invokes fn once per stored component.
	 * @param fn Callback receiving (Entity, const void*) for each stored component.
	 */
	void ForEachRaw(const std::function<void(virasa::ecs::Entity, const void*)>& fn) const override;

	/**
	 * @brief Returns the dirty entity vector.
	 * @return Reference to the dirty vector.
	 */
	[[nodiscard]] const std::vector<virasa::ecs::Entity>& Dirty() const noexcept override;

	/**
	 * @brief Removes one entity from the dirty set.
	 * @param entity The entity to clear.
	 */
	void ClearDirty(virasa::ecs::Entity entity) override;

	/// @brief Empties the dirty set.
	void ClearAllDirty() override;

	/// @brief No-op for the base SparseComponentSystem; subclasses override to drain dirty state.
	void Update() override;

	/**
	 * @brief Creates an independent SparseComponentSystem with copied base storage.
	 * @return New ComponentSystem instance with copied sparse-set storage.
	 */
	[[nodiscard]] std::unique_ptr<virasa::ecs::ComponentSystem> Clone() const override;

protected:
	/**
	 * @brief Copies the base sparse-set storage into a compatible target system.
	 * @param target Target system that receives dense data, dense entities, sparse indices, and dirty set.
	 */
	void CopyStorageInto(virasa::ecs::SparseComponentSystem& target) const;

	/**
	 * Adds entity to the dirty set if not already present.
	 * Available to subclasses that need to flag additional entities.
	 */
	void MarkDirty(virasa::ecs::Entity entity);

	/**
	 * Called after a new component is appended at denseIndex.
	 * Subclasses override to append a corresponding derived slot.
	 */
	virtual void OnAdd(uint32_t denseIndex);

	/**
	 * Called during swap-and-pop before the dense collections shrink.
	 * Subclasses override to apply the identical swap-and-pop to parallel arrays.
	 */
	virtual void OnRemoveSwap(uint32_t removedDenseIndex, uint32_t lastDenseIndex);

	/**
	 * Called after a value is overwritten via SetRaw.
	 * Subclasses override to flag derived state stale.
	 */
	virtual void OnSet(virasa::ecs::Entity entity);

private:
	static constexpr uint32_t kSparseSentinel = 0xFFFFFFFFu;

	virasa::ecs::ComponentId _id;
	const char* _name;
	size_t _elementSize;
	virasa::ecs::SystemLayer _layer;

	std::vector<uint8_t> _denseData;       // tightly packed component bytes
	std::vector<virasa::ecs::Entity> _denseEntities; // parallel dense entity list
	std::vector<uint32_t> _sparse;         // entity.index -> dense slot (kSparseSentinel = none)
	std::vector<virasa::ecs::Entity> _dirty; // entities marked dirty since last clear
};

} // namespace virasa::ecs

#endif // VIRASA_ECS_COMPONENTSYSTEM_H
