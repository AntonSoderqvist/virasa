#ifndef VIRASA_ECS_TRANSFORMSYSTEM_H
#define VIRASA_ECS_TRANSFORMSYSTEM_H

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Types.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace virasa::ecs
{

/**
 * @brief Concrete ComponentSystem that stores a local-space Transform per entity
 *        and caches the corresponding world-space model matrix as a Mat4.
 *
 * TransformSystem derives from SparseComponentSystem, using the base's byte-backed
 * sparse set as storage for the local virasa::math::Transform values. In addition,
 * it maintains a parallel dense std::vector of virasa::math::Mat4 holding the cached
 * world matrix for each dense slot, in one-to-one correspondence with the base's
 * dense slots.
 *
 * World-matrix propagation is NOT owned by this system; the virasa::ecs::World drives
 * propagation by reading the Dirty() set and calling SetWorld() after composing each
 * entity's world matrix from the hierarchy.
 */
class TransformSystem final : public virasa::ecs::SparseComponentSystem
{
public:
	/**
	 * @brief Construct a TransformSystem with the given ComponentId.
	 * @param id The ComponentId assigned by the World.
	 */
	explicit TransformSystem(virasa::ecs::ComponentId id);

	/// @brief Virtual destructor.
	virtual ~TransformSystem() noexcept = default;

	/**
	 * @brief Insert a local Transform for an entity that does not yet have one.
	 * @param entity The entity to add the component to.
	 * @param local  The initial local-space Transform.
	 */
	void Add(virasa::ecs::Entity entity, const virasa::math::Transform& local);

	/**
	 * @brief Get the stored local Transform for an entity.
	 * @param entity The entity to query.
	 * @return Const reference to the entity's local Transform.
	 */
	[[nodiscard]] const virasa::math::Transform& GetLocal(virasa::ecs::Entity entity) const;

	/**
	 * @brief Overwrite the stored local Transform for an entity (write-through, marks dirty).
	 * @param entity The entity to update.
	 * @param local  The new local-space Transform.
	 */
	void SetLocal(virasa::ecs::Entity entity, const virasa::math::Transform& local);

	/**
	 * @brief Get the cached world-space matrix for an entity.
	 * @param entity The entity to query.
	 * @return Const reference to the entity's cached world Mat4.
	 */
	[[nodiscard]] const virasa::math::Mat4& GetWorld(virasa::ecs::Entity entity) const;

	/**
	 * @brief Overwrite the cached world-space matrix for an entity.
	 *
	 * Intended to be called by the propagation owner (World) after composing
	 * the entity's world matrix. Does NOT mark the entity dirty.
	 *
	 * @param entity The entity to update.
	 * @param world  The new world-space Mat4.
	 */
	void SetWorld(virasa::ecs::Entity entity, const virasa::math::Mat4& world);

	/**
	 * @brief Extract the world-space position (translation column) from the cached world matrix.
	 * @param entity The entity to query.
	 * @return World-space position as a Vec3.
	 */
	[[nodiscard]] virasa::math::Vec3 GetWorldPosition(virasa::ecs::Entity entity) const;

	/**
	 * @brief Extract the normalized world-space forward direction (+Y local axis) from the cached world matrix.
	 * @param entity The entity to query.
	 * @return Normalized world-space forward direction as a Vec3.
	 */
	[[nodiscard]] virasa::math::Vec3 GetWorldForward(virasa::ecs::Entity entity) const;

	/**
	 * @brief Creates an independent TransformSystem with copied local storage and world cache.
	 * @return New ComponentSystem instance with copied TransformSystem state.
	 */
	[[nodiscard]] std::unique_ptr<virasa::ecs::ComponentSystem> Clone() const override;

protected:
	/// @brief Hook called after a new local Transform is appended; appends an identity world matrix.
	void OnAdd(uint32_t denseIndex) override;

	/// @brief Hook called during swap-and-pop removal; mirrors the operation on the world-matrix vector.
	void OnRemoveSwap(uint32_t removedDenseIndex, uint32_t lastDenseIndex) override;

private:
	std::vector<virasa::math::Mat4> _worldMatrices;
};

} // namespace virasa::ecs

#endif // VIRASA_ECS_TRANSFORMSYSTEM_H
