#include <gtest/gtest.h>

#include "virasa/input/Resolver.h"

#include <cstddef>
#include <type_traits>
#include <utility>

using namespace virasa::input;
using namespace virasa::window;

namespace
{

virasa::Event MakeKeyDown(KeyCode key)
{
	virasa::Event event{};
	event.type = virasa::EventType::KeyDown;
	event.keyboard.key = key;
	event.keyboard.repeat = false;
	return event;
}

virasa::Event MakeKeyUp(KeyCode key)
{
	virasa::Event event{};
	event.type = virasa::EventType::KeyUp;
	event.keyboard.key = key;
	event.keyboard.repeat = false;
	return event;
}

virasa::Event MakeMouseButtonDown(MouseButton button)
{
	virasa::Event event{};
	event.type = virasa::EventType::MouseButtonDown;
	event.mouseButton.button = button;
	event.mouseButton.x = 0;
	event.mouseButton.y = 0;
	return event;
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

} // namespace

TEST(Resolver, test_resolver_is_a_stateless_pure_input_to_action_mapper)
{
	static_assert(std::is_final_v<Resolver>);
	static_assert(std::is_copy_constructible_v<Resolver>);
	static_assert(std::is_copy_assignable_v<Resolver>);
	static_assert(std::is_move_constructible_v<Resolver>);
	static_assert(std::is_move_assignable_v<Resolver>);
	static_assert(std::is_default_constructible_v<Resolver>);
	static_assert(std::is_same_v<
		decltype(std::declval<const Resolver&>().Resolve(
			std::declval<const InputState&>(),
			std::declval<const Bindings&>())),
		ActionState>);

	InputState input;
	Bindings bindings;
	const Resolver resolver;

	const ActionState empty = resolver.Resolve(input, bindings);
	for (std::size_t index = 0; index < ActionState::kMaxActions; ++index)
	{
		const ActionId action = static_cast<ActionId>(index);
		EXPECT_FLOAT_EQ(empty.Axis(action), 0.0f);
		EXPECT_FALSE(empty.Held(action));
		EXPECT_FALSE(empty.Pressed(action));
		EXPECT_FALSE(empty.Released(action));
	}

	const virasa::Event events[] = {MakeKeyDown(KeyCode::Space)};
	input.Update(events);
	bindings.BindDigital(3u, KeyCode::Space);
	bindings.BindAxis(4u, KeyCode::Space, KeyCode::A, 0.5f);

	const ActionState first = resolver.Resolve(input, bindings);
	const ActionState second = resolver.Resolve(input, bindings);
	ExpectActionStateEq(first, second);

	EXPECT_TRUE(input.IsKeyDown(KeyCode::Space));
	EXPECT_TRUE(input.WasKeyPressed(KeyCode::Space));
	ASSERT_EQ(bindings.DigitalBindings().size(), 1u);
	ASSERT_EQ(bindings.AxisBindings().size(), 1u);
	EXPECT_EQ(bindings.DigitalBindings()[0].action, 3u);
	EXPECT_EQ(bindings.DigitalBindings()[0].key, KeyCode::Space);
	EXPECT_EQ(bindings.AxisBindings()[0].action, 4u);
	EXPECT_EQ(bindings.AxisBindings()[0].positive, KeyCode::Space);
	EXPECT_EQ(bindings.AxisBindings()[0].negative, KeyCode::A);
	EXPECT_FLOAT_EQ(bindings.AxisBindings()[0].scale, 0.5f);

	Resolver copy = resolver;
	Resolver moved = std::move(copy);
	ExpectActionStateEq(first, moved.Resolve(input, bindings));
}

TEST(Resolver, test_resolve_composes_action_state_from_bindings)
{
	const ActionId jump = 1u;
	const ActionId fire = 2u;
	const ActionId unbound = 3u;
	const ActionId move = 4u;
	const ActionId negativeMove = 5u;
	const ActionId unknownAxis = 6u;
	const ActionId cancelledAxis = 7u;
	const ActionId hybrid = 8u;
	const ActionId firstOutOfRange = static_cast<ActionId>(ActionState::kMaxActions);

	InputState input;
	const virasa::Event previousFrame[] = {MakeKeyDown(KeyCode::Escape)};
	input.Update(previousFrame);

	const virasa::Event currentFrame[] = {
		MakeKeyUp(KeyCode::Escape),
		MakeKeyDown(KeyCode::Space),
		MakeMouseButtonDown(MouseButton::Left),
		MakeKeyDown(KeyCode::D),
		MakeKeyDown(KeyCode::Right),
		MakeKeyDown(KeyCode::S),
		MakeKeyDown(KeyCode::Down),
		MakeKeyDown(KeyCode::Q),
		MakeKeyDown(KeyCode::E),
	};
	input.Update(currentFrame);

	Bindings bindings;
	bindings.BindDigital(jump, KeyCode::Space);
	bindings.BindDigital(jump, KeyCode::Escape);
	bindings.BindDigitalMouse(fire, MouseButton::Left);
	bindings.BindDigital(hybrid, KeyCode::Space);
	bindings.BindDigital(ActionState::kInvalidActionId, KeyCode::Space);
	bindings.BindDigital(firstOutOfRange, KeyCode::Space);

	bindings.BindAxis(move, KeyCode::D, KeyCode::A, 0.75f);
	bindings.BindAxis(move, KeyCode::Right, KeyCode::Left, 0.75f);
	bindings.BindAxis(negativeMove, KeyCode::Unknown, KeyCode::S, 0.75f);
	bindings.BindAxis(negativeMove, KeyCode::Unknown, KeyCode::Down, 0.75f);
	bindings.BindAxis(unknownAxis, KeyCode::Unknown, KeyCode::Unknown, 1.0f);
	bindings.BindAxis(cancelledAxis, KeyCode::Q, KeyCode::E, 1.0f);
	bindings.BindAxis(hybrid, KeyCode::D, KeyCode::A, 0.25f);
	bindings.BindAxis(ActionState::kInvalidActionId, KeyCode::D, KeyCode::Unknown, 8.0f);
	bindings.BindAxis(firstOutOfRange, KeyCode::D, KeyCode::Unknown, 8.0f);

	const ActionState state = Resolver{}.Resolve(input, bindings);

	EXPECT_TRUE(state.Held(jump));
	EXPECT_TRUE(state.Pressed(jump));
	EXPECT_TRUE(state.Released(jump));
	EXPECT_FLOAT_EQ(state.Axis(jump), 0.0f);

	EXPECT_TRUE(state.Held(fire));
	EXPECT_FALSE(state.Pressed(fire));
	EXPECT_FALSE(state.Released(fire));
	EXPECT_FLOAT_EQ(state.Axis(fire), 0.0f);

	EXPECT_FALSE(state.Held(unbound));
	EXPECT_FALSE(state.Pressed(unbound));
	EXPECT_FALSE(state.Released(unbound));
	EXPECT_FLOAT_EQ(state.Axis(unbound), 0.0f);

	EXPECT_FALSE(state.Held(move));
	EXPECT_FALSE(state.Pressed(move));
	EXPECT_FALSE(state.Released(move));
	EXPECT_FLOAT_EQ(state.Axis(move), 1.0f);

	EXPECT_FLOAT_EQ(state.Axis(negativeMove), -1.0f);
	EXPECT_FLOAT_EQ(state.Axis(unknownAxis), 0.0f);
	EXPECT_FLOAT_EQ(state.Axis(cancelledAxis), 0.0f);

	EXPECT_TRUE(state.Held(hybrid));
	EXPECT_TRUE(state.Pressed(hybrid));
	EXPECT_FALSE(state.Released(hybrid));
	EXPECT_FLOAT_EQ(state.Axis(hybrid), 0.25f);

	EXPECT_FALSE(state.Held(ActionState::kInvalidActionId));
	EXPECT_FALSE(state.Pressed(ActionState::kInvalidActionId));
	EXPECT_FALSE(state.Released(ActionState::kInvalidActionId));
	EXPECT_FLOAT_EQ(state.Axis(ActionState::kInvalidActionId), 0.0f);
	EXPECT_FALSE(state.Held(firstOutOfRange));
	EXPECT_FALSE(state.Pressed(firstOutOfRange));
	EXPECT_FALSE(state.Released(firstOutOfRange));
	EXPECT_FLOAT_EQ(state.Axis(firstOutOfRange), 0.0f);
}
