#include <gtest/gtest.h>

#include "virasa/sim/Builtins.h"

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/World.h"
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

	RegisterGameplayComponents(world);

	const ComponentId spinId = world.GetSystemId("Spin");
	ASSERT_NE(spinId, kInvalidComponentId);
	ComponentSystem& spinSystem = world.GetSystem(spinId);
	EXPECT_STREQ(spinSystem.Name(), "Spin");
	EXPECT_EQ(spinSystem.Id(), spinId);
	EXPECT_EQ(spinSystem.Size(), 0u);

	Entity entity = world.CreateEntity("Spinner");
	SpinComponent spin;
	spin.angularVelocity = virasa::math::Vec3(0.0f, 0.0f, 1.0f);
	spinSystem.AddRaw(entity, &spin);

	RegisterGameplayComponents(world);

	EXPECT_EQ(world.GetSystemId("Spin"), spinId);
	EXPECT_EQ(&world.GetSystem(spinId), &spinSystem);
	EXPECT_EQ(spinSystem.Size(), 1u);
	EXPECT_TRUE(spinSystem.Has(entity));
}

TEST(Builtins, test_register_builtin_behaviors_populates_registry)
{
	BehaviorRegistry registry;
	RegisterBuiltinBehaviors(registry);

	EXPECT_TRUE(registry.Contains("Spin"));
	EXPECT_EQ(registry.Size(), 1u);

	const std::vector<std::string> expectedNames = {"Spin"};
	EXPECT_EQ(registry.Names(), expectedNames);

	Scheduler first;
	Scheduler second;
	EXPECT_TRUE(registry.Build("Spin", first));
	EXPECT_TRUE(registry.Build("Spin", second));
	EXPECT_EQ(first.BehaviorCount(Phase::PreStep), 0u);
	EXPECT_EQ(first.BehaviorCount(Phase::Step), 1u);
	EXPECT_EQ(first.BehaviorCount(Phase::PostStep), 0u);
	EXPECT_EQ(second.BehaviorCount(Phase::Step), 1u);
}
