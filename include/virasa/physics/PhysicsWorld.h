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
 * @brief Reports the closest hit from a ray or swept-shape cast.
 *
 * A default-constructed CastHit denotes a miss. The remaining fields are
 * meaningful only when hit is true.
 */
struct CastHit
{
public:
	/// @brief True when the cast struck a body.
	bool hit = false;

	/// @brief Entity that owns the hit body, or Entity::Invalid() when unresolved.
	virasa::ecs::Entity entity = virasa::ecs::Entity::Invalid();

	/// @brief World-space contact position.
	virasa::math::Vec3 point = virasa::math::Vec3(0.0f, 0.0f, 0.0f);

	/// @brief World-space contact normal pointing back toward the cast origin.
	virasa::math::Vec3 normal = virasa::math::Vec3(0.0f, 0.0f, 1.0f);

	/// @brief Hit distance as a fraction of the requested cast distance.
	float fraction = 0.0f;
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

	/**
	 * @brief Casts a ray and returns the closest blocking hit.
	 * @param origin World-space ray start.
	 * @param direction Ray direction; it does not need to be normalized.
	 * @param maxDistance Maximum cast distance in world units.
	 * @param ignore Entity whose body should be excluded from the cast.
	 * @return Closest hit, or a default miss result when nothing is struck.
	 */
	[[nodiscard]] virasa::physics::CastHit RayCast(
		const virasa::math::Vec3& origin,
		const virasa::math::Vec3& direction,
		float maxDistance,
		virasa::ecs::Entity ignore) const;

	/**
	 * @brief Sweeps a sphere and returns the closest blocking hit.
	 * @param origin World-space sphere centre at the start of the cast.
	 * @param radius Sphere radius in world units.
	 * @param direction Cast direction; it does not need to be normalized.
	 * @param maxDistance Maximum cast distance in world units.
	 * @param ignore Entity whose body should be excluded from the cast.
	 * @return Closest hit, or a default miss result when nothing is struck.
	 */
	[[nodiscard]] virasa::physics::CastHit SphereCast(
		const virasa::math::Vec3& origin,
		float radius,
		const virasa::math::Vec3& direction,
		float maxDistance,
		virasa::ecs::Entity ignore) const;

	/**
	 * @brief Applies a world-space force through a body's centre of mass.
	 * @param entity Entity whose body receives the force.
	 * @param force Force in newtons.
	 */
	void AddForce(virasa::ecs::Entity entity, const virasa::math::Vec3& force);

	/**
	 * @brief Applies a world-space force at a world-space point.
	 * @param entity Entity whose body receives the force.
	 * @param force Force in newtons.
	 * @param worldPoint World-space point where the force is applied.
	 */
	void AddForceAtPoint(
		virasa::ecs::Entity entity,
		const virasa::math::Vec3& force,
		const virasa::math::Vec3& worldPoint);

	/**
	 * @brief Applies a world-space torque about a body's centre of mass.
	 * @param entity Entity whose body receives the torque.
	 * @param torque Torque in newton-metres.
	 */
	void AddTorque(virasa::ecs::Entity entity, const virasa::math::Vec3& torque);

	/**
	 * @brief Returns a body's world-space linear velocity.
	 * @param entity Entity whose body should be queried.
	 * @return Linear velocity in metres per second, or zero when no body exists.
	 */
	[[nodiscard]] virasa::math::Vec3 GetLinearVelocity(
		virasa::ecs::Entity entity) const;

	/**
	 * @brief Returns the world-space velocity of a material point on a body.
	 * @param entity Entity whose body should be queried.
	 * @param worldPoint World-space point on or relative to the body.
	 * @return Point velocity in metres per second, or zero when no body exists.
	 */
	[[nodiscard]] virasa::math::Vec3 GetPointVelocity(
		virasa::ecs::Entity entity,
		const virasa::math::Vec3& worldPoint) const;

private:
	struct Impl;
	std::unique_ptr<Impl> _impl;
};

} // namespace virasa::physics

#endif // VIRASA_PHYSICS_PHYSICSWORLD_H
