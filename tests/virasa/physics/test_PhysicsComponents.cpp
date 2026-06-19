#include <gtest/gtest.h>

#include "virasa/physics/PhysicsComponents.h"

#include <cstdint>
#include <type_traits>
#include <utility>

TEST(PhysicsComponents, test_body_type_classifies_simulation_participation)
{
	EXPECT_TRUE((std::is_same_v<std::underlying_type_t<virasa::physics::BodyType>, uint8_t>));
	EXPECT_EQ(sizeof(virasa::physics::BodyType), sizeof(uint8_t));

	EXPECT_EQ(static_cast<uint8_t>(virasa::physics::BodyType::Static), uint8_t{0});
	EXPECT_EQ(static_cast<uint8_t>(virasa::physics::BodyType::Kinematic), uint8_t{1});
	EXPECT_EQ(static_cast<uint8_t>(virasa::physics::BodyType::Dynamic), uint8_t{2});

	EXPECT_NE(virasa::physics::BodyType::Static, virasa::physics::BodyType::Kinematic);
	EXPECT_NE(virasa::physics::BodyType::Static, virasa::physics::BodyType::Dynamic);
	EXPECT_NE(virasa::physics::BodyType::Kinematic, virasa::physics::BodyType::Dynamic);
}

TEST(PhysicsComponents, test_collider_shape_selects_primitive_geometry)
{
	EXPECT_TRUE((std::is_same_v<std::underlying_type_t<virasa::physics::ColliderShape>, uint8_t>));
	EXPECT_EQ(sizeof(virasa::physics::ColliderShape), sizeof(uint8_t));

	EXPECT_EQ(static_cast<uint8_t>(virasa::physics::ColliderShape::Box), uint8_t{0});
	EXPECT_EQ(static_cast<uint8_t>(virasa::physics::ColliderShape::Sphere), uint8_t{1});
	EXPECT_EQ(static_cast<uint8_t>(virasa::physics::ColliderShape::Capsule), uint8_t{2});

	EXPECT_NE(virasa::physics::ColliderShape::Box, virasa::physics::ColliderShape::Sphere);
	EXPECT_NE(virasa::physics::ColliderShape::Box, virasa::physics::ColliderShape::Capsule);
	EXPECT_NE(virasa::physics::ColliderShape::Sphere, virasa::physics::ColliderShape::Capsule);
}

TEST(PhysicsComponents, test_rigid_body_component_holds_authored_body_parameters)
{
	virasa::physics::RigidBodyComponent defaultComp;
	EXPECT_EQ(defaultComp.bodyType, virasa::physics::BodyType::Dynamic);
	EXPECT_FLOAT_EQ(defaultComp.mass, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.linearDamping, 0.05f);
	EXPECT_FLOAT_EQ(defaultComp.angularDamping, 0.05f);
	EXPECT_FLOAT_EQ(defaultComp.friction, 0.2f);
	EXPECT_FLOAT_EQ(defaultComp.restitution, 0.0f);
	EXPECT_FLOAT_EQ(defaultComp.gravityFactor, 1.0f);

	virasa::physics::RigidBodyComponent comp;
	comp.bodyType = virasa::physics::BodyType::Kinematic;
	comp.mass = 4.0f;
	comp.linearDamping = 0.15f;
	comp.angularDamping = 0.25f;
	comp.friction = 0.75f;
	comp.restitution = 0.5f;
	comp.gravityFactor = 0.0f;

	EXPECT_EQ(comp.bodyType, virasa::physics::BodyType::Kinematic);
	EXPECT_FLOAT_EQ(comp.mass, 4.0f);
	EXPECT_FLOAT_EQ(comp.linearDamping, 0.15f);
	EXPECT_FLOAT_EQ(comp.angularDamping, 0.25f);
	EXPECT_FLOAT_EQ(comp.friction, 0.75f);
	EXPECT_FLOAT_EQ(comp.restitution, 0.5f);
	EXPECT_FLOAT_EQ(comp.gravityFactor, 0.0f);

	virasa::physics::RigidBodyComponent copy = comp;
	EXPECT_EQ(copy.bodyType, comp.bodyType);
	EXPECT_FLOAT_EQ(copy.mass, comp.mass);
	EXPECT_FLOAT_EQ(copy.linearDamping, comp.linearDamping);
	EXPECT_FLOAT_EQ(copy.angularDamping, comp.angularDamping);
	EXPECT_FLOAT_EQ(copy.friction, comp.friction);
	EXPECT_FLOAT_EQ(copy.restitution, comp.restitution);
	EXPECT_FLOAT_EQ(copy.gravityFactor, comp.gravityFactor);

	virasa::physics::RigidBodyComponent assigned;
	assigned = comp;
	EXPECT_EQ(assigned.bodyType, comp.bodyType);
	EXPECT_FLOAT_EQ(assigned.mass, comp.mass);
	EXPECT_FLOAT_EQ(assigned.linearDamping, comp.linearDamping);
	EXPECT_FLOAT_EQ(assigned.angularDamping, comp.angularDamping);
	EXPECT_FLOAT_EQ(assigned.friction, comp.friction);
	EXPECT_FLOAT_EQ(assigned.restitution, comp.restitution);
	EXPECT_FLOAT_EQ(assigned.gravityFactor, comp.gravityFactor);

	virasa::physics::RigidBodyComponent moved = std::move(copy);
	EXPECT_EQ(moved.bodyType, comp.bodyType);
	EXPECT_FLOAT_EQ(moved.mass, comp.mass);
	EXPECT_FLOAT_EQ(moved.linearDamping, comp.linearDamping);
	EXPECT_FLOAT_EQ(moved.angularDamping, comp.angularDamping);
	EXPECT_FLOAT_EQ(moved.friction, comp.friction);
	EXPECT_FLOAT_EQ(moved.restitution, comp.restitution);
	EXPECT_FLOAT_EQ(moved.gravityFactor, comp.gravityFactor);

	EXPECT_TRUE(std::is_default_constructible_v<virasa::physics::RigidBodyComponent>);
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::physics::RigidBodyComponent>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::physics::RigidBodyComponent>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::physics::RigidBodyComponent>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::physics::RigidBodyComponent>);
	EXPECT_TRUE(std::is_trivially_destructible_v<virasa::physics::RigidBodyComponent>);
	EXPECT_TRUE(std::is_trivially_copyable_v<virasa::physics::RigidBodyComponent>);
}

TEST(PhysicsComponents, test_collider_component_describes_local_collision_volume)
{
	virasa::physics::ColliderComponent defaultComp;
	EXPECT_EQ(defaultComp.shape, virasa::physics::ColliderShape::Box);
	EXPECT_FLOAT_EQ(defaultComp.halfExtents.x, 0.5f);
	EXPECT_FLOAT_EQ(defaultComp.halfExtents.y, 0.5f);
	EXPECT_FLOAT_EQ(defaultComp.halfExtents.z, 0.5f);
	EXPECT_FLOAT_EQ(defaultComp.radius, 0.5f);
	EXPECT_FLOAT_EQ(defaultComp.halfHeight, 0.5f);
	EXPECT_FLOAT_EQ(defaultComp.offset.x, 0.0f);
	EXPECT_FLOAT_EQ(defaultComp.offset.y, 0.0f);
	EXPECT_FLOAT_EQ(defaultComp.offset.z, 0.0f);

	virasa::physics::ColliderComponent comp;
	comp.shape = virasa::physics::ColliderShape::Capsule;
	comp.halfExtents = virasa::math::Vec3(1.0f, 2.0f, 3.0f);
	comp.radius = 0.75f;
	comp.halfHeight = 2.5f;
	comp.offset = virasa::math::Vec3(-1.0f, 0.5f, 4.0f);

	EXPECT_EQ(comp.shape, virasa::physics::ColliderShape::Capsule);
	EXPECT_FLOAT_EQ(comp.halfExtents.x, 1.0f);
	EXPECT_FLOAT_EQ(comp.halfExtents.y, 2.0f);
	EXPECT_FLOAT_EQ(comp.halfExtents.z, 3.0f);
	EXPECT_FLOAT_EQ(comp.radius, 0.75f);
	EXPECT_FLOAT_EQ(comp.halfHeight, 2.5f);
	EXPECT_FLOAT_EQ(comp.offset.x, -1.0f);
	EXPECT_FLOAT_EQ(comp.offset.y, 0.5f);
	EXPECT_FLOAT_EQ(comp.offset.z, 4.0f);

	virasa::physics::ColliderComponent copy = comp;
	EXPECT_EQ(copy.shape, comp.shape);
	EXPECT_FLOAT_EQ(copy.halfExtents.x, comp.halfExtents.x);
	EXPECT_FLOAT_EQ(copy.halfExtents.y, comp.halfExtents.y);
	EXPECT_FLOAT_EQ(copy.halfExtents.z, comp.halfExtents.z);
	EXPECT_FLOAT_EQ(copy.radius, comp.radius);
	EXPECT_FLOAT_EQ(copy.halfHeight, comp.halfHeight);
	EXPECT_FLOAT_EQ(copy.offset.x, comp.offset.x);
	EXPECT_FLOAT_EQ(copy.offset.y, comp.offset.y);
	EXPECT_FLOAT_EQ(copy.offset.z, comp.offset.z);

	virasa::physics::ColliderComponent assigned;
	assigned = comp;
	EXPECT_EQ(assigned.shape, comp.shape);
	EXPECT_FLOAT_EQ(assigned.halfExtents.x, comp.halfExtents.x);
	EXPECT_FLOAT_EQ(assigned.halfExtents.y, comp.halfExtents.y);
	EXPECT_FLOAT_EQ(assigned.halfExtents.z, comp.halfExtents.z);
	EXPECT_FLOAT_EQ(assigned.radius, comp.radius);
	EXPECT_FLOAT_EQ(assigned.halfHeight, comp.halfHeight);
	EXPECT_FLOAT_EQ(assigned.offset.x, comp.offset.x);
	EXPECT_FLOAT_EQ(assigned.offset.y, comp.offset.y);
	EXPECT_FLOAT_EQ(assigned.offset.z, comp.offset.z);

	virasa::physics::ColliderComponent moved = std::move(copy);
	EXPECT_EQ(moved.shape, comp.shape);
	EXPECT_FLOAT_EQ(moved.halfExtents.x, comp.halfExtents.x);
	EXPECT_FLOAT_EQ(moved.halfExtents.y, comp.halfExtents.y);
	EXPECT_FLOAT_EQ(moved.halfExtents.z, comp.halfExtents.z);
	EXPECT_FLOAT_EQ(moved.radius, comp.radius);
	EXPECT_FLOAT_EQ(moved.halfHeight, comp.halfHeight);
	EXPECT_FLOAT_EQ(moved.offset.x, comp.offset.x);
	EXPECT_FLOAT_EQ(moved.offset.y, comp.offset.y);
	EXPECT_FLOAT_EQ(moved.offset.z, comp.offset.z);

	EXPECT_TRUE(std::is_default_constructible_v<virasa::physics::ColliderComponent>);
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::physics::ColliderComponent>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::physics::ColliderComponent>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::physics::ColliderComponent>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::physics::ColliderComponent>);
	EXPECT_TRUE(std::is_trivially_destructible_v<virasa::physics::ColliderComponent>);
	EXPECT_TRUE(std::is_trivially_copyable_v<virasa::physics::ColliderComponent>);
}
