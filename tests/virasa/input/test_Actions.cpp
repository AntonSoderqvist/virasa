#include <gtest/gtest.h>

#include "virasa/input/Actions.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

using namespace virasa::input;

TEST(ActionState, test_action_id_is_dense_index_handle)
{
	static_assert(std::is_same_v<ActionId, std::uint16_t>);
	static_assert(std::is_same_v<decltype(ActionState::kMaxActions), const std::size_t>);
	static_assert(std::is_same_v<decltype(ActionState::kInvalidActionId), const ActionId>);

	EXPECT_EQ(ActionState::kMaxActions, std::size_t{64u});
	EXPECT_EQ(ActionState::kInvalidActionId, static_cast<ActionId>(0xFFFFu));
	EXPECT_GE(static_cast<std::size_t>(ActionState::kInvalidActionId), ActionState::kMaxActions);

	const ActionId firstAction = 0u;
	const ActionId lastAction = static_cast<ActionId>(ActionState::kMaxActions - 1u);
	const ActionId firstOutOfRange = static_cast<ActionId>(ActionState::kMaxActions);

	EXPECT_LT(static_cast<std::size_t>(firstAction), ActionState::kMaxActions);
	EXPECT_LT(static_cast<std::size_t>(lastAction), ActionState::kMaxActions);
	EXPECT_GE(static_cast<std::size_t>(firstOutOfRange), ActionState::kMaxActions);
}

TEST(ActionState, test_action_state_is_per_step_input_snapshot)
{
	static_assert(std::is_final_v<ActionState>);
	static_assert(std::is_trivially_copyable_v<ActionState>);
	static_assert(std::is_copy_constructible_v<ActionState>);
	static_assert(std::is_copy_assignable_v<ActionState>);
	static_assert(std::is_move_constructible_v<ActionState>);
	static_assert(std::is_move_assignable_v<ActionState>);
	static_assert(std::is_default_constructible_v<ActionState>);

	ActionState state;
	for (std::size_t index = 0; index < ActionState::kMaxActions; ++index)
	{
		const ActionId action = static_cast<ActionId>(index);
		EXPECT_FLOAT_EQ(state.Axis(action), 0.0f);
		EXPECT_FALSE(state.Held(action));
		EXPECT_FALSE(state.Pressed(action));
		EXPECT_FALSE(state.Released(action));
	}

	const ActionId throttle = 3u;
	const ActionId reset = 7u;
	state.SetAxis(throttle, 0.75f);
	state.SetButton(reset, true, true, false);

	ActionState copy = state;
	EXPECT_FLOAT_EQ(copy.Axis(throttle), 0.75f);
	EXPECT_TRUE(copy.Held(reset));
	EXPECT_TRUE(copy.Pressed(reset));
	EXPECT_FALSE(copy.Released(reset));

	ActionState moved = std::move(copy);
	EXPECT_FLOAT_EQ(moved.Axis(throttle), 0.75f);
	EXPECT_TRUE(moved.Held(reset));
	EXPECT_TRUE(moved.Pressed(reset));
	EXPECT_FALSE(moved.Released(reset));

	state.Clear();
	for (std::size_t index = 0; index < ActionState::kMaxActions; ++index)
	{
		const ActionId action = static_cast<ActionId>(index);
		EXPECT_FLOAT_EQ(state.Axis(action), 0.0f);
		EXPECT_FALSE(state.Held(action));
		EXPECT_FALSE(state.Pressed(action));
		EXPECT_FALSE(state.Released(action));
	}
}

TEST(ActionState, test_action_state_queries_are_bounds_checked_and_setters_clamp_range)
{
	static_assert(noexcept(std::declval<const ActionState&>().Axis(ActionId{})));
	static_assert(noexcept(std::declval<const ActionState&>().Held(ActionId{})));
	static_assert(noexcept(std::declval<const ActionState&>().Pressed(ActionId{})));
	static_assert(noexcept(std::declval<const ActionState&>().Released(ActionId{})));

	ActionState state;
	const ActionId valid = 5u;
	const ActionId lastValid = static_cast<ActionId>(ActionState::kMaxActions - 1u);
	const ActionId firstOutOfRange = static_cast<ActionId>(ActionState::kMaxActions);
	const ActionId invalid = ActionState::kInvalidActionId;

	state.SetAxis(valid, 1.5f);
	state.SetButton(valid, true, false, true);

	const ActionState& constState = state;
	EXPECT_FLOAT_EQ(constState.Axis(valid), 1.5f);
	EXPECT_TRUE(constState.Held(valid));
	EXPECT_FALSE(constState.Pressed(valid));
	EXPECT_TRUE(constState.Released(valid));

	state.SetAxis(valid, -2.0f);
	EXPECT_FLOAT_EQ(state.Axis(valid), -2.0f);
	EXPECT_TRUE(state.Held(valid));
	EXPECT_FALSE(state.Pressed(valid));
	EXPECT_TRUE(state.Released(valid));

	state.SetButton(valid, false, true, false);
	EXPECT_FLOAT_EQ(state.Axis(valid), -2.0f);
	EXPECT_FALSE(state.Held(valid));
	EXPECT_TRUE(state.Pressed(valid));
	EXPECT_FALSE(state.Released(valid));

	state.SetAxis(lastValid, 0.25f);
	state.SetButton(lastValid, true, true, true);
	EXPECT_FLOAT_EQ(state.Axis(lastValid), 0.25f);
	EXPECT_TRUE(state.Held(lastValid));
	EXPECT_TRUE(state.Pressed(lastValid));
	EXPECT_TRUE(state.Released(lastValid));

	state.SetAxis(firstOutOfRange, 0.5f);
	state.SetButton(firstOutOfRange, true, true, true);
	state.SetAxis(invalid, 0.5f);
	state.SetButton(invalid, true, true, true);

	EXPECT_FLOAT_EQ(state.Axis(firstOutOfRange), 0.0f);
	EXPECT_FALSE(state.Held(firstOutOfRange));
	EXPECT_FALSE(state.Pressed(firstOutOfRange));
	EXPECT_FALSE(state.Released(firstOutOfRange));
	EXPECT_FLOAT_EQ(state.Axis(invalid), 0.0f);
	EXPECT_FALSE(state.Held(invalid));
	EXPECT_FALSE(state.Pressed(invalid));
	EXPECT_FALSE(state.Released(invalid));

	EXPECT_FLOAT_EQ(state.Axis(valid), -2.0f);
	EXPECT_FALSE(state.Held(valid));
	EXPECT_TRUE(state.Pressed(valid));
	EXPECT_FALSE(state.Released(valid));
	EXPECT_FLOAT_EQ(state.Axis(lastValid), 0.25f);
	EXPECT_TRUE(state.Held(lastValid));
	EXPECT_TRUE(state.Pressed(lastValid));
	EXPECT_TRUE(state.Released(lastValid));
}
