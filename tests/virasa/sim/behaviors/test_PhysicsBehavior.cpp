#include <gtest/gtest.h>

#include "virasa/sim/behaviors/PhysicsBehavior.h"

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/TransformSystem.h"
#include "virasa/ecs/World.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"
#include "virasa/physics/PhysicsComponents.h"
#include "virasa/physics/PhysicsWorld.h"
#include "virasa/sim/Tick.h"

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>

using namespace virasa::ecs;
using namespace virasa::math;
using namespace virasa::physics;
using namespace virasa::sim;
using namespace virasa::sim::behaviors;

namespace
{

constexpr float kEps = 1e-5f;

void RegisterPhysicsSystems(World& world)
{
	(void)world.RegisterSystem(std::make_unique<SparseComponentSystem>(
		kInvalidComponentId,
		"RigidBody",
		sizeof(RigidBodyComponent)));
	(void)world.RegisterSystem(std::make_unique<SparseComponentSystem>(
		kInvalidComponentId,
		"Collider",
		sizeof(ColliderComponent)));
}

Transform MakeTransform(const Vec3& translation)
{
	Transform transform;
	transform.translation = translation;
	return transform;
}

RigidBodyComponent MakeRigidBody(BodyType bodyType)
{
	RigidBodyComponent body;
	body.bodyType = bodyType;
	return body;
}

ColliderComponent MakeBox(const Vec3& halfExtents)
{
	ColliderComponent collider;
	collider.shape = ColliderShape::Box;
	collider.halfExtents = halfExtents;
	return collider;
}

void AddPhysicsComponents(
	World& world,
	Entity entity,
	const RigidBodyComponent& body,
	const ColliderComponent& collider)
{
	const ComponentId rigidBodyId = world.GetSystemId("RigidBody");
	const ComponentId colliderId = world.GetSystemId("Collider");
	ASSERT_NE(rigidBodyId, kInvalidComponentId);
	ASSERT_NE(colliderId, kInvalidComponentId);

	world.GetSystem(rigidBodyId).AddRaw(entity, &body);
	world.GetSystem(colliderId).AddRaw(entity, &collider);
}

void ExpectVec3Near(const Vec3& actual, const Vec3& expected, float tolerance)
{
	EXPECT_NEAR(actual.x, expected.x, tolerance);
	EXPECT_NEAR(actual.y, expected.y, tolerance);
	EXPECT_NEAR(actual.z, expected.z, tolerance);
}

} // namespace

TEST(PhysicsBehavior, test_physics_behavior_is_concrete_physics_system)
{
	static_assert(std::is_final_v<PhysicsBehavior>);
	static_assert(std::is_base_of_v<Behavior, PhysicsBehavior>);
	static_assert(std::is_default_constructible_v<PhysicsBehavior>);
	static_assert(!std::is_copy_constructible_v<PhysicsBehavior>);
	static_assert(!std::is_copy_assignable_v<PhysicsBehavior>);
	static_assert(!std::is_move_constructible_v<PhysicsBehavior>);
	static_assert(!std::is_move_assignable_v<PhysicsBehavior>);
	static_assert(std::has_virtual_destructor_v<PhysicsBehavior>);

	std::unique_ptr<Behavior> behavior = std::make_unique<PhysicsBehavior>();
	EXPECT_EQ(std::string(behavior->Name()), "Physics");
	EXPECT_STREQ(behavior->Name(), "Physics");
}

TEST(PhysicsBehavior, test_physics_behavior_lazily_populates_bodies_on_first_step)
{
	PhysicsBehavior behavior;
	TickContext tick;
	tick.deltaTime = 1.0f / 60.0f;

	World world;
	Entity entity = world.CreateEntity("LaterPhysicsBody");
	const Transform initial = MakeTransform(Vec3(0.0f, 0.0f, 5.0f));
	world.Transforms().Add(entity, initial);

	CommandBuffer missingCommands;
	behavior.Step(world, tick, missingCommands);

	EXPECT_TRUE(missingCommands.IsEmpty());
	ExpectVec3Near(world.Transforms().GetLocal(entity).translation, initial.translation, kEps);

	RegisterPhysicsSystems(world);
	AddPhysicsComponents(
		world,
		entity,
		MakeRigidBody(BodyType::Dynamic),
		MakeBox(Vec3(0.5f, 0.5f, 0.5f)));

	CommandBuffer firstCommands;
	behavior.Step(world, tick, firstCommands);

	const float firstStepZ = world.Transforms().GetLocal(entity).translation.z;
	EXPECT_TRUE(firstCommands.IsEmpty());
	EXPECT_LT(firstStepZ, initial.translation.z);

	Entity lateEntity = world.CreateEntity("LatePhysicsBody");
	const Transform lateInitial = MakeTransform(Vec3(0.0f, 0.0f, 8.0f));
	world.Transforms().Add(lateEntity, lateInitial);
	AddPhysicsComponents(
		world,
		lateEntity,
		MakeRigidBody(BodyType::Dynamic),
		MakeBox(Vec3(0.5f, 0.5f, 0.5f)));

	CommandBuffer secondCommands;
	behavior.Step(world, tick, secondCommands);

	EXPECT_TRUE(secondCommands.IsEmpty());
	EXPECT_LT(world.Transforms().GetLocal(entity).translation.z, firstStepZ);
	ExpectVec3Near(
		world.Transforms().GetLocal(lateEntity).translation,
		lateInitial.translation,
		kEps);
}

TEST(PhysicsBehavior, test_physics_behavior_steps_and_syncs_each_tick)
{
	PhysicsBehavior behavior;
	TickContext tick;
	tick.deltaTime = 1.0f / 60.0f;

	World world;
	RegisterPhysicsSystems(world);

	Entity floorEntity = world.CreateEntity("PhysicsFloor");
	const Transform floorTransform = MakeTransform(Vec3(0.0f, 0.0f, -0.1f));
	world.Transforms().Add(floorEntity, floorTransform);
	AddPhysicsComponents(
		world,
		floorEntity,
		MakeRigidBody(BodyType::Static),
		MakeBox(Vec3(4.0f, 4.0f, 0.1f)));

	Entity boxEntity = world.CreateEntity("PhysicsBox");
	const Transform boxInitial = MakeTransform(Vec3(0.0f, 0.0f, 4.0f));
	world.Transforms().Add(boxEntity, boxInitial);
	AddPhysicsComponents(
		world,
		boxEntity,
		MakeRigidBody(BodyType::Dynamic),
		MakeBox(Vec3(0.5f, 0.5f, 0.5f)));

	const uint32_t entityCountBefore = world.GetEntityCount();
	const size_t rigidBodyCountBefore = world.GetSystem(world.GetSystemId("RigidBody")).Size();
	const size_t colliderCountBefore = world.GetSystem(world.GetSystemId("Collider")).Size();

	CommandBuffer commands;
	behavior.Step(world, tick, commands);

	const float afterFirstStepZ = world.Transforms().GetLocal(boxEntity).translation.z;
	EXPECT_TRUE(commands.IsEmpty());
	EXPECT_EQ(world.GetEntityCount(), entityCountBefore);
	EXPECT_EQ(world.GetSystem(world.GetSystemId("RigidBody")).Size(), rigidBodyCountBefore);
	EXPECT_EQ(world.GetSystem(world.GetSystemId("Collider")).Size(), colliderCountBefore);
	EXPECT_LT(afterFirstStepZ, boxInitial.translation.z);
	ExpectVec3Near(
		world.Transforms().GetLocal(floorEntity).translation,
		floorTransform.translation,
		kEps);

	TickContext zeroTick;
	zeroTick.deltaTime = 0.0f;

	CommandBuffer zeroCommands;
	behavior.Step(world, zeroTick, zeroCommands);

	const float afterZeroStepZ = world.Transforms().GetLocal(boxEntity).translation.z;
	EXPECT_TRUE(zeroCommands.IsEmpty());
	EXPECT_NEAR(afterZeroStepZ, afterFirstStepZ, kEps);

	CommandBuffer secondCommands;
	behavior.Step(world, tick, secondCommands);

	EXPECT_TRUE(secondCommands.IsEmpty());
	EXPECT_LT(world.Transforms().GetLocal(boxEntity).translation.z, afterZeroStepZ);
}

TEST(PhysicsBehavior, test_physics_behavior_publishes_world_to_resource_store)
{
	PhysicsBehavior behavior;
	TickContext tick;
	tick.deltaTime = 1.0f / 60.0f;

	World world;
	RegisterPhysicsSystems(world);

	Entity entity = world.CreateEntity("PublishedPhysicsBody");
	world.Transforms().Add(entity, MakeTransform(Vec3(0.0f, 0.0f, 1.0f)));
	AddPhysicsComponents(
		world,
		entity,
		MakeRigidBody(BodyType::Dynamic),
		MakeBox(Vec3(0.5f, 0.5f, 0.5f)));

	const auto resourceKey = std::type_index(typeid(PhysicsWorld));
	EXPECT_EQ(world.GetResource(resourceKey), nullptr);

	CommandBuffer commands;
	behavior.Step(world, tick, commands);

	void* published = world.GetResource(resourceKey);
	ASSERT_NE(published, nullptr);
	EXPECT_NE(static_cast<PhysicsWorld*>(published), nullptr);
	EXPECT_TRUE(commands.IsEmpty());
}
