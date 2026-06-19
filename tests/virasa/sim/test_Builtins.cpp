#include <gtest/gtest.h>

#include "virasa/sim/Builtins.h"

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/World.h"
#include "virasa/physics/PhysicsComponents.h"
#include "virasa/sim/BehaviorRegistry.h"
#include "virasa/sim/GameplayComponents.h"

#include <string>
#include <vector>

using namespace virasa::ecs;
using namespace virasa::sim;

TEST(Builtins, test_register_gameplay_components_registers_sim_systems)
{
	World world;
	EXPECT_EQ(world.GetSystemId("Spin"), kInvalidComponentId);
	EXPECT_EQ(world.GetSystemId("RigidBody"), kInvalidComponentId);
	EXPECT_EQ(world.GetSystemId("Collider"), kInvalidComponentId);

	RegisterGameplayComponents(world);

	const ComponentId spinId = world.GetSystemId("Spin");
	const ComponentId rigidBodyId = world.GetSystemId("RigidBody");
	const ComponentId colliderId = world.GetSystemId("Collider");
	ASSERT_NE(spinId, kInvalidComponentId);
	ASSERT_NE(rigidBodyId, kInvalidComponentId);
	ASSERT_NE(colliderId, kInvalidComponentId);

	ComponentSystem& spinSystem = world.GetSystem(spinId);
	ComponentSystem& rigidBodySystem = world.GetSystem(rigidBodyId);
	ComponentSystem& colliderSystem = world.GetSystem(colliderId);
	EXPECT_STREQ(spinSystem.Name(), "Spin");
	EXPECT_STREQ(rigidBodySystem.Name(), "RigidBody");
	EXPECT_STREQ(colliderSystem.Name(), "Collider");
	EXPECT_EQ(spinSystem.Id(), spinId);
	EXPECT_EQ(rigidBodySystem.Id(), rigidBodyId);
	EXPECT_EQ(colliderSystem.Id(), colliderId);
	EXPECT_EQ(spinSystem.Size(), 0u);
	EXPECT_EQ(rigidBodySystem.Size(), 0u);
	EXPECT_EQ(colliderSystem.Size(), 0u);

	Entity entity = world.CreateEntity("Spinner");
	SpinComponent spin;
	spin.angularVelocity = virasa::math::Vec3(0.0f, 0.0f, 1.0f);
	virasa::physics::RigidBodyComponent rigidBody;
	rigidBody.mass = 2.0f;
	virasa::physics::ColliderComponent collider;
	collider.radius = 1.5f;
	spinSystem.AddRaw(entity, &spin);
	rigidBodySystem.AddRaw(entity, &rigidBody);
	colliderSystem.AddRaw(entity, &collider);

	RegisterGameplayComponents(world);

	EXPECT_EQ(world.GetSystemId("Spin"), spinId);
	EXPECT_EQ(world.GetSystemId("RigidBody"), rigidBodyId);
	EXPECT_EQ(world.GetSystemId("Collider"), colliderId);
	EXPECT_EQ(&world.GetSystem(spinId), &spinSystem);
	EXPECT_EQ(&world.GetSystem(rigidBodyId), &rigidBodySystem);
	EXPECT_EQ(&world.GetSystem(colliderId), &colliderSystem);
	EXPECT_EQ(spinSystem.Size(), 1u);
	EXPECT_EQ(rigidBodySystem.Size(), 1u);
	EXPECT_EQ(colliderSystem.Size(), 1u);
	EXPECT_TRUE(spinSystem.Has(entity));
	EXPECT_TRUE(rigidBodySystem.Has(entity));
	EXPECT_TRUE(colliderSystem.Has(entity));
}

TEST(Builtins, test_register_builtin_behaviors_populates_registry)
{
	BehaviorRegistry registry;
	RegisterBuiltinBehaviors(registry);

	EXPECT_TRUE(registry.Contains("Spin"));
	EXPECT_TRUE(registry.Contains("Physics"));
	EXPECT_EQ(registry.Size(), 2u);

	const std::vector<std::string> expectedNames = {"Spin", "Physics"};
	EXPECT_EQ(registry.Names(), expectedNames);

	Scheduler first;
	Scheduler second;
	EXPECT_TRUE(registry.Build("Spin", first));
	EXPECT_TRUE(registry.Build("Physics", first));
	EXPECT_TRUE(registry.Build("Spin", second));
	EXPECT_TRUE(registry.Build("Physics", second));
	EXPECT_EQ(first.BehaviorCount(Phase::PreStep), 0u);
	EXPECT_EQ(first.BehaviorCount(Phase::Step), 2u);
	EXPECT_EQ(first.BehaviorCount(Phase::PostStep), 0u);
	EXPECT_EQ(second.BehaviorCount(Phase::PreStep), 0u);
	EXPECT_EQ(second.BehaviorCount(Phase::Step), 2u);
	EXPECT_EQ(second.BehaviorCount(Phase::PostStep), 0u);
}
