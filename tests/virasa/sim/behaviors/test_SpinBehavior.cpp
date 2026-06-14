#include <gtest/gtest.h>

#include "virasa/sim/behaviors/SpinBehavior.h"

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/TransformSystem.h"
#include "virasa/ecs/World.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"
#include "virasa/sim/GameplayComponents.h"
#include "virasa/sim/Tick.h"

#include "glm/gtc/quaternion.hpp"

#include <memory>
#include <string>
#include <type_traits>

using namespace virasa::ecs;
using namespace virasa::math;
using namespace virasa::sim;
using namespace virasa::sim::behaviors;

namespace
{

constexpr float kEps = 1e-5f;

void RegisterSpinSystem(World& world)
{
	(void)world.RegisterSystem(std::make_unique<SparseComponentSystem>(
		kInvalidComponentId,
		"Spin",
		sizeof(SpinComponent)));
}

} // namespace

TEST(SpinBehavior, test_spin_behavior_is_concrete_spin_system)
{
	static_assert(std::is_final_v<SpinBehavior>);
	static_assert(std::is_base_of_v<Behavior, SpinBehavior>);
	static_assert(std::is_default_constructible_v<SpinBehavior>);
	static_assert(!std::is_copy_constructible_v<SpinBehavior>);
	static_assert(!std::is_copy_assignable_v<SpinBehavior>);
	static_assert(!std::is_move_constructible_v<SpinBehavior>);
	static_assert(!std::is_move_assignable_v<SpinBehavior>);

	std::unique_ptr<Behavior> behavior = std::make_unique<SpinBehavior>();
	EXPECT_EQ(std::string(behavior->Name()), "Spin");
	EXPECT_STREQ(behavior->Name(), "Spin");
}

TEST(SpinBehavior, test_spin_behavior_integrates_local_rotation)
{
	SpinBehavior behavior;
	TickContext tick;
	tick.deltaTime = 0.5f;

	World missingSpinWorld;
	Entity noSpinEntity = missingSpinWorld.CreateEntity("NoSpinSystem");
	Transform noSpinInitial;
	noSpinInitial.rotation = glm::angleAxis(0.25f, Vec3(1.0f, 0.0f, 0.0f));
	missingSpinWorld.Transforms().Add(noSpinEntity, noSpinInitial);

	CommandBuffer missingCommands;
	behavior.Step(missingSpinWorld, tick, missingCommands);

	EXPECT_TRUE(missingCommands.IsEmpty());
	EXPECT_NEAR(
		missingSpinWorld.Transforms().GetLocal(noSpinEntity).rotation.w,
		noSpinInitial.rotation.w,
		kEps);
	EXPECT_NEAR(
		missingSpinWorld.Transforms().GetLocal(noSpinEntity).rotation.x,
		noSpinInitial.rotation.x,
		kEps);

	World world;
	RegisterSpinSystem(world);

	const ComponentId spinId = world.GetSystemId("Spin");
	ASSERT_NE(spinId, kInvalidComponentId);

	Entity spinning = world.CreateEntity("Spinning");
	Transform local;
	local.translation = Vec3(3.0f, 4.0f, 5.0f);
	local.rotation = glm::angleAxis(0.25f, Vec3(1.0f, 0.0f, 0.0f));
	local.scale = Vec3(2.0f, 3.0f, 4.0f);
	world.Transforms().Add(spinning, local);

	SpinComponent spin;
	spin.angularVelocity = Vec3(0.0f, 0.0f, 2.0f);
	world.GetSystem(spinId).AddRaw(spinning, &spin);

	Entity idle = world.CreateEntity("Idle");
	Transform idleLocal;
	idleLocal.rotation = glm::angleAxis(0.4f, Vec3(0.0f, 1.0f, 0.0f));
	world.Transforms().Add(idle, idleLocal);

	SpinComponent zeroSpin;
	world.GetSystem(spinId).AddRaw(idle, &zeroSpin);

	CommandBuffer commands;
	behavior.Step(world, tick, commands);

	const Quat expected = glm::normalize(
		local.rotation * glm::angleAxis(1.0f, Vec3(0.0f, 0.0f, 1.0f)));
	const Transform& updated = world.Transforms().GetLocal(spinning);

	EXPECT_TRUE(commands.IsEmpty());
	EXPECT_NEAR(updated.translation.x, 3.0f, kEps);
	EXPECT_NEAR(updated.translation.y, 4.0f, kEps);
	EXPECT_NEAR(updated.translation.z, 5.0f, kEps);
	EXPECT_NEAR(updated.scale.x, 2.0f, kEps);
	EXPECT_NEAR(updated.scale.y, 3.0f, kEps);
	EXPECT_NEAR(updated.scale.z, 4.0f, kEps);
	EXPECT_NEAR(updated.rotation.w, expected.w, kEps);
	EXPECT_NEAR(updated.rotation.x, expected.x, kEps);
	EXPECT_NEAR(updated.rotation.y, expected.y, kEps);
	EXPECT_NEAR(updated.rotation.z, expected.z, kEps);
	EXPECT_NEAR(glm::length(updated.rotation), 1.0f, kEps);

	const Transform& idleUpdated = world.Transforms().GetLocal(idle);
	EXPECT_NEAR(idleUpdated.rotation.w, idleLocal.rotation.w, kEps);
	EXPECT_NEAR(idleUpdated.rotation.x, idleLocal.rotation.x, kEps);
	EXPECT_NEAR(idleUpdated.rotation.y, idleLocal.rotation.y, kEps);
	EXPECT_NEAR(idleUpdated.rotation.z, idleLocal.rotation.z, kEps);
}
