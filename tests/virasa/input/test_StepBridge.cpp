#include <gtest/gtest.h>

#include "virasa/input/StepBridge.h"
#include "virasa/ecs/World.h"

#include <cstddef>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>

using namespace virasa::ecs;
using namespace virasa::input;

namespace
{

ActionState MakePatternedActions(float axisBase, bool pressed, bool released)
{
	ActionState actions;

	for (std::size_t index = 0; index < ActionState::kMaxActions; ++index)
	{
		const ActionId action = static_cast<ActionId>(index);
		actions.SetAxis(action, axisBase + static_cast<float>(index) * 0.25f);
		actions.SetButton(action, (index % 2u) == 0u, pressed, released);
	}

	return actions;
}

void ExpectActionStateEq(const ActionState& lhs, const ActionState& rhs)
{
	for (std::size_t index = 0; index < ActionState::kMaxActions; ++index)
	{
		const ActionId action = static_cast<ActionId>(index);
		EXPECT_FLOAT_EQ(lhs.Axis(action), rhs.Axis(action));
		EXPECT_EQ(lhs.Held(action), rhs.Held(action));
		EXPECT_EQ(lhs.Pressed(action), rhs.Pressed(action));
		EXPECT_EQ(lhs.Released(action), rhs.Released(action));
	}
}

void ExpectContinuousEqAndEdgesCleared(const ActionState& step, const ActionState& latched)
{
	for (std::size_t index = 0; index < ActionState::kMaxActions; ++index)
	{
		const ActionId action = static_cast<ActionId>(index);
		EXPECT_FLOAT_EQ(step.Axis(action), latched.Axis(action));
		EXPECT_EQ(step.Held(action), latched.Held(action));
		EXPECT_FALSE(step.Pressed(action));
		EXPECT_FALSE(step.Released(action));
	}
}

} // namespace

TEST(StepBridge, test_step_bridge_latches_per_frame_actions)
{
	static_assert(std::is_final_v<StepBridge>);
	static_assert(std::is_default_constructible_v<StepBridge>);
	static_assert(!std::is_copy_constructible_v<StepBridge>);
	static_assert(!std::is_copy_assignable_v<StepBridge>);
	static_assert(!std::is_move_constructible_v<StepBridge>);
	static_assert(!std::is_move_assignable_v<StepBridge>);
	static_assert(std::is_same_v<
		decltype(std::declval<StepBridge&>().LatchFrame(std::declval<const ActionState&>())),
		void>);

	StepBridge bridge;
	World world;

	bridge.Publish(world, true);
	const ActionState* initial = StepBridge::GetActions(world);
	ASSERT_NE(initial, nullptr);

	for (std::size_t index = 0; index < ActionState::kMaxActions; ++index)
	{
		const ActionId action = static_cast<ActionId>(index);
		EXPECT_FLOAT_EQ(initial->Axis(action), 0.0f);
		EXPECT_FALSE(initial->Held(action));
		EXPECT_FALSE(initial->Pressed(action));
		EXPECT_FALSE(initial->Released(action));
	}

	const ActionState firstFrame = MakePatternedActions(1.0f, true, false);
	bridge.LatchFrame(firstFrame);
	EXPECT_EQ(StepBridge::GetActions(world), initial);
	ExpectActionStateEq(*initial, ActionState{});

	bridge.Publish(world, true);
	const ActionState* firstStep = StepBridge::GetActions(world);
	ASSERT_NE(firstStep, nullptr);
	ExpectActionStateEq(*firstStep, firstFrame);

	const ActionState replacementFrame = MakePatternedActions(-4.0f, false, true);
	bridge.LatchFrame(replacementFrame);
	EXPECT_EQ(StepBridge::GetActions(world), firstStep);
	ExpectActionStateEq(*firstStep, firstFrame);

	bridge.Publish(world, true);
	EXPECT_EQ(StepBridge::GetActions(world), firstStep);
	ExpectActionStateEq(*firstStep, replacementFrame);
}

TEST(StepBridge, test_step_bridge_publishes_edge_once_per_frame_into_world_resource)
{
	StepBridge bridge;
	World world;
	const ActionState frameActions = MakePatternedActions(-2.0f, true, true);

	bridge.LatchFrame(frameActions);
	bridge.Publish(world, true);

	const auto resourceKey = std::type_index(typeid(ActionState));
	const ActionState* firstStep = StepBridge::GetActions(world);
	ASSERT_NE(firstStep, nullptr);
	EXPECT_EQ(world.GetResource(resourceKey), firstStep);
	ExpectActionStateEq(*firstStep, frameActions);

	bridge.Publish(world, false);

	const ActionState* laterStep = StepBridge::GetActions(world);
	ASSERT_NE(laterStep, nullptr);
	EXPECT_EQ(laterStep, firstStep);
	EXPECT_EQ(world.GetResource(resourceKey), laterStep);
	ExpectContinuousEqAndEdgesCleared(*laterStep, frameActions);

	bridge.Publish(world, false);

	const ActionState* anotherLaterStep = StepBridge::GetActions(world);
	ASSERT_NE(anotherLaterStep, nullptr);
	EXPECT_EQ(anotherLaterStep, firstStep);
	ExpectContinuousEqAndEdgesCleared(*anotherLaterStep, frameActions);
}

TEST(StepBridge, test_step_bridge_get_actions_reads_world_resource)
{
	static_assert(noexcept(StepBridge::GetActions(std::declval<const World&>())));
	static_assert(std::is_same_v<
		decltype(StepBridge::GetActions(std::declval<const World&>())),
		const ActionState*>);

	World world;
	EXPECT_EQ(StepBridge::GetActions(world), nullptr);

	ActionState externalActions;
	externalActions.SetAxis(3u, 0.5f);
	externalActions.SetButton(3u, true, true, false);
	world.SetResource(std::type_index(typeid(ActionState)), &externalActions);

	const ActionState* fromWorld = StepBridge::GetActions(world);
	ASSERT_EQ(fromWorld, &externalActions);
	EXPECT_FLOAT_EQ(fromWorld->Axis(3u), 0.5f);
	EXPECT_TRUE(fromWorld->Held(3u));
	EXPECT_TRUE(fromWorld->Pressed(3u));
	EXPECT_FALSE(fromWorld->Released(3u));

	StepBridge bridge;
	bridge.Publish(world, true);

	const ActionState* published = StepBridge::GetActions(world);
	ASSERT_NE(published, nullptr);
	EXPECT_NE(published, &externalActions);
	EXPECT_EQ(world.GetResource(std::type_index(typeid(ActionState))), published);
	ExpectActionStateEq(*published, ActionState{});

	world.RemoveResource(std::type_index(typeid(ActionState)));
	EXPECT_EQ(StepBridge::GetActions(world), nullptr);
}
