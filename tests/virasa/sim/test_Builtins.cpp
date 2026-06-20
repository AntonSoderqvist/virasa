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
	EXPECT_EQ(world.GetSystemId("Vehicle"), kInvalidComponentId);
	EXPECT_EQ(world.GetSystemId("ChaseCamera"), kInvalidComponentId);

	RegisterGameplayComponents(world);

	const ComponentId spinId = world.GetSystemId("Spin");
	const ComponentId rigidBodyId = world.GetSystemId("RigidBody");
	const ComponentId colliderId = world.GetSystemId("Collider");
	const ComponentId vehicleId = world.GetSystemId("Vehicle");
	const ComponentId chaseCameraId = world.GetSystemId("ChaseCamera");
	ASSERT_NE(spinId, kInvalidComponentId);
	ASSERT_NE(rigidBodyId, kInvalidComponentId);
	ASSERT_NE(colliderId, kInvalidComponentId);
	ASSERT_NE(vehicleId, kInvalidComponentId);
	ASSERT_NE(chaseCameraId, kInvalidComponentId);

	ComponentSystem& spinSystem = world.GetSystem(spinId);
	ComponentSystem& rigidBodySystem = world.GetSystem(rigidBodyId);
	ComponentSystem& colliderSystem = world.GetSystem(colliderId);
	ComponentSystem& vehicleSystem = world.GetSystem(vehicleId);
	ComponentSystem& chaseCameraSystem = world.GetSystem(chaseCameraId);
	EXPECT_STREQ(spinSystem.Name(), "Spin");
	EXPECT_STREQ(rigidBodySystem.Name(), "RigidBody");
	EXPECT_STREQ(colliderSystem.Name(), "Collider");
	EXPECT_STREQ(vehicleSystem.Name(), "Vehicle");
	EXPECT_STREQ(chaseCameraSystem.Name(), "ChaseCamera");
	EXPECT_EQ(spinSystem.Id(), spinId);
	EXPECT_EQ(rigidBodySystem.Id(), rigidBodyId);
	EXPECT_EQ(colliderSystem.Id(), colliderId);
	EXPECT_EQ(vehicleSystem.Id(), vehicleId);
	EXPECT_EQ(chaseCameraSystem.Id(), chaseCameraId);
	EXPECT_EQ(spinSystem.Size(), 0u);
	EXPECT_EQ(rigidBodySystem.Size(), 0u);
	EXPECT_EQ(colliderSystem.Size(), 0u);
	EXPECT_EQ(vehicleSystem.Size(), 0u);
	EXPECT_EQ(chaseCameraSystem.Size(), 0u);

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
	EXPECT_EQ(world.GetSystemId("Vehicle"), vehicleId);
	EXPECT_EQ(world.GetSystemId("ChaseCamera"), chaseCameraId);
	EXPECT_EQ(&world.GetSystem(spinId), &spinSystem);
	EXPECT_EQ(&world.GetSystem(rigidBodyId), &rigidBodySystem);
	EXPECT_EQ(&world.GetSystem(colliderId), &colliderSystem);
	EXPECT_EQ(&world.GetSystem(vehicleId), &vehicleSystem);
	EXPECT_EQ(&world.GetSystem(chaseCameraId), &chaseCameraSystem);
	EXPECT_EQ(spinSystem.Size(), 1u);
	EXPECT_EQ(rigidBodySystem.Size(), 1u);
	EXPECT_EQ(colliderSystem.Size(), 1u);
	EXPECT_EQ(vehicleSystem.Size(), 0u);
	EXPECT_EQ(chaseCameraSystem.Size(), 0u);
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
	EXPECT_TRUE(registry.Contains("Vehicle"));
	EXPECT_TRUE(registry.Contains("ChaseCamera"));
	EXPECT_EQ(registry.Size(), 4u);

	const std::vector<std::string> expectedNames = {"Spin", "Physics", "Vehicle", "ChaseCamera"};
	EXPECT_EQ(registry.Names(), expectedNames);

	Scheduler first;
	Scheduler second;
	EXPECT_TRUE(registry.Build("Spin", first));
	EXPECT_TRUE(registry.Build("Physics", first));
	EXPECT_TRUE(registry.Build("Vehicle", first));
	EXPECT_TRUE(registry.Build("ChaseCamera", first));
	EXPECT_TRUE(registry.Build("Spin", second));
	EXPECT_TRUE(registry.Build("Physics", second));
	EXPECT_TRUE(registry.Build("Vehicle", second));
	EXPECT_TRUE(registry.Build("ChaseCamera", second));
	EXPECT_EQ(first.BehaviorCount(Phase::PreStep), 1u);
	EXPECT_EQ(first.BehaviorCount(Phase::Step), 2u);
	EXPECT_EQ(first.BehaviorCount(Phase::PostStep), 1u);
	EXPECT_EQ(second.BehaviorCount(Phase::PreStep), 1u);
	EXPECT_EQ(second.BehaviorCount(Phase::Step), 2u);
	EXPECT_EQ(second.BehaviorCount(Phase::PostStep), 1u);

	Scheduler vehicleOnly;
	EXPECT_TRUE(registry.Build("Vehicle", vehicleOnly));
	EXPECT_EQ(vehicleOnly.BehaviorCount(Phase::PreStep), 1u);
	EXPECT_EQ(vehicleOnly.BehaviorCount(Phase::Step), 0u);
	EXPECT_EQ(vehicleOnly.BehaviorCount(Phase::PostStep), 0u);

	Scheduler chaseCameraOnly;
	EXPECT_TRUE(registry.Build("ChaseCamera", chaseCameraOnly));
	EXPECT_EQ(chaseCameraOnly.BehaviorCount(Phase::PreStep), 0u);
	EXPECT_EQ(chaseCameraOnly.BehaviorCount(Phase::Step), 0u);
	EXPECT_EQ(chaseCameraOnly.BehaviorCount(Phase::PostStep), 1u);
}
