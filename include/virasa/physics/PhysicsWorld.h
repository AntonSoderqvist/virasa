#ifndef VIRASA_PHYSICS_PHYSICSWORLD_H
#define VIRASA_PHYSICS_PHYSICSWORLD_H

#include "virasa/ecs/Types.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"
#include "virasa/physics/PhysicsComponents.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace virasa::ecs
{
class World;
}

namespace virasa::physics
{

/**
 * @brief Fixed construction parameters for a PhysicsWorld.
 *
 * Gravity is expressed in the engine's Z-up coordinate space. Capacity values
 * are passed to the underlying rigid-body simulation during construction.
 */
struct PhysicsConfig
{
public:
	/// @brief World-space gravitational acceleration in metres per second squared.
	virasa::math::Vec3 gravity = virasa::math::Vec3(0.0f, 0.0f, -9.81f);

	/// @brief Maximum number of bodies the simulation may allocate.
	uint32_t maxBodies = 65536u;

	/// @brief Maximum queued broad-phase body pairs.
	uint32_t maxBodyPairs = 65536u;

	/// @brief Maximum contact constraints allocated by the solver.
	uint32_t maxContactConstraints = 10240u;
};

/**
 * @brief Owns one rigid-body simulation and maps ECS entities to bodies.
 *
 * PhysicsWorld keeps all third-party physics types behind an opaque implementation
 * pointer. Instances are not thread-safe and must be accessed from one thread at a time.
 */
class PhysicsWorld final
{
public:
	/**
	 * @brief Constructs a ready-to-run empty physics simulation.
	 * @param config Fixed gravity and capacity settings for the simulation.
	 */
	explicit PhysicsWorld(const virasa::physics::PhysicsConfig& config);

	/// @brief Destroys all owned physics bodies and simulation resources.
	~PhysicsWorld() noexcept;

	PhysicsWorld(const PhysicsWorld&) = delete;
	PhysicsWorld& operator=(const PhysicsWorld&) = delete;
	PhysicsWorld(PhysicsWorld&&) = delete;
	PhysicsWorld& operator=(PhysicsWorld&&) = delete;

	/**
	 * @brief Creates a rigid body for an entity and stores the entity-to-body mapping.
	 * @param entity Entity that will own the runtime body.
	 * @param body Authored body simulation properties.
	 * @param collider Authored primitive collider properties.
	 * @param transform Initial world-space body transform; scale is ignored.
	 */
	void AddBody(
		virasa::ecs::Entity entity,
		const virasa::physics::RigidBodyComponent& body,
		const virasa::physics::ColliderComponent& collider,
		const virasa::math::Transform& transform);

	/**
	 * @brief Removes and destroys the body associated with an entity if present.
	 * @param entity Entity whose body should be removed.
	 */
	void RemoveBody(virasa::ecs::Entity entity);

	/**
	 * @brief Returns true when the entity currently has a body in this simulation.
	 * @param entity Entity handle to query.
	 * @return true if a mapped body exists.
	 */
	[[nodiscard]] bool HasBody(virasa::ecs::Entity entity) const noexcept;

	/**
	 * @brief Returns the number of live bodies owned by this PhysicsWorld.
	 * @return Number of entity-to-body mappings.
	 */
	[[nodiscard]] size_t BodyCount() const noexcept;

	/**
	 * @brief Sets an existing body's world-space position and orientation.
	 * @param entity Entity whose body should be moved.
	 * @param transform New pose; scale is ignored.
	 */
	void SetBodyTransform(
		virasa::ecs::Entity entity,
		const virasa::math::Transform& transform);

	/**
	 * @brief Reads an existing body's world-space transform.
	 * @param entity Entity whose body should be queried.
	 * @return Current body pose, or identity if the entity has no body.
	 */
	[[nodiscard]] virasa::math::Transform GetBodyTransform(
		virasa::ecs::Entity entity) const;

	/**
	 * @brief Advances the simulation by one update with one collision step.
	 * @param deltaTime Step duration in seconds; non-positive values are no-ops.
	 */
	void Step(float deltaTime);

	/**
	 * @brief Writes all simulated body poses into matching Transform components.
	 * @param world ECS world whose TransformSystem receives the simulated poses.
	 */
	void SyncToWorld(virasa::ecs::World& world) const;

private:
	struct Impl;
	std::unique_ptr<Impl> _impl;
};

} // namespace virasa::physics

#endif // VIRASA_PHYSICS_PHYSICSWORLD_H
