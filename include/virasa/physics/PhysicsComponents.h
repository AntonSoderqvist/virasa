#ifndef VIRASA_PHYSICS_PHYSICSCOMPONENTS_H
#define VIRASA_PHYSICS_PHYSICSCOMPONENTS_H

#include "virasa/math/Types.h"

#include <cstdint>

namespace virasa::physics
{

/**
 * @brief Classifies how a rigid body participates in physics simulation.
 *
 * BodyType values are serialized as their pinned uint8_t encodings and are
 * consumed by PhysicsWorld when creating the corresponding runtime body.
 */
enum class BodyType : uint8_t
{
	Static = 0,		///< Immovable collision geometry that is never integrated.
	Kinematic = 1,	///< Application-authored motion unaffected by forces or contacts.
	Dynamic = 2,	///< Fully simulated body integrated by gravity and contact forces.
};

/**
 * @brief Selects the primitive collision volume described by a collider component.
 *
 * ColliderShape values are serialized as their pinned uint8_t encodings and are
 * consumed by PhysicsWorld when building the corresponding runtime shape.
 */
enum class ColliderShape : uint8_t
{
	Box = 0,	 ///< Axis-aligned local box using ColliderComponent::halfExtents.
	Sphere = 1,	 ///< Sphere using ColliderComponent::radius.
	Capsule = 2, ///< Z-axis capsule using ColliderComponent::radius and halfHeight.
};

/**
 * @brief Authored dynamic properties for an entity's rigid body.
 *
 * RigidBodyComponent carries only authoring input. Runtime physics handles and
 * simulated state are owned by PhysicsWorld.
 */
struct RigidBodyComponent
{
public:
	/// @brief Simulation participation mode for this body.
	virasa::physics::BodyType bodyType = virasa::physics::BodyType::Dynamic;

	/// @brief Body mass in kilograms, used only for dynamic bodies.
	float mass = 1.0f;

	/// @brief Per-second linear velocity damping fraction.
	float linearDamping = 0.05f;

	/// @brief Per-second angular velocity damping fraction.
	float angularDamping = 0.05f;

	/// @brief Dimensionless contact friction coefficient.
	float friction = 0.2f;

	/// @brief Dimensionless contact restitution coefficient.
	float restitution = 0.0f;

	/// @brief Scale applied to world gravity for this body.
	float gravityFactor = 1.0f;
};

/**
 * @brief Authored local-space collision volume for an entity's rigid body.
 *
 * ColliderComponent carries only primitive shape input. Runtime shape objects
 * are created and owned by PhysicsWorld.
 */
struct ColliderComponent
{
public:
	/// @brief Primitive shape selected for this collider.
	virasa::physics::ColliderShape shape = virasa::physics::ColliderShape::Box;

	/// @brief Half-dimensions of the local box shape.
	virasa::math::Vec3 halfExtents = virasa::math::Vec3(0.5f, 0.5f, 0.5f);

	/// @brief Sphere radius or capsule cap radius.
	float radius = 0.5f;

	/// @brief Half-length of the capsule central cylinder.
	float halfHeight = 0.5f;

	/// @brief Local-space translation of the shape relative to the body origin.
	virasa::math::Vec3 offset = virasa::math::Vec3(0.0f, 0.0f, 0.0f);
};

} // namespace virasa::physics

#endif // VIRASA_PHYSICS_PHYSICSCOMPONENTS_H
