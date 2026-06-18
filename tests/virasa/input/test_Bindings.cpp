#include <gtest/gtest.h>

#include "virasa/input/Bindings.h"

#include <span>
#include <type_traits>
#include <utility>

using namespace virasa::input;
using namespace virasa::window;

namespace
{

float AxisContribution(const AxisBinding& binding, bool positiveDown, bool negativeDown)
{
	const float positive = positiveDown ? 1.0f : 0.0f;
	const float negative = negativeDown ? 1.0f : 0.0f;
	return (positive - negative) * binding.scale;
}

} // namespace

TEST(Bindings, test_digital_binding_maps_one_control_to_an_action)
{
	static_assert(std::is_copy_constructible_v<DigitalBinding>);
	static_assert(std::is_copy_assignable_v<DigitalBinding>);
	static_assert(std::is_move_constructible_v<DigitalBinding>);
	static_assert(std::is_move_assignable_v<DigitalBinding>);
	static_assert(std::is_default_constructible_v<DigitalBinding>);

	DigitalBinding keyboardBinding;
	keyboardBinding.action = 3u;
	keyboardBinding.key = KeyCode::Space;
	keyboardBinding.button = MouseButton::Right;
	keyboardBinding.useMouse = false;

	EXPECT_EQ(keyboardBinding.action, 3u);
	EXPECT_EQ(keyboardBinding.key, KeyCode::Space);
	EXPECT_EQ(keyboardBinding.button, MouseButton::Right);
	EXPECT_FALSE(keyboardBinding.useMouse);

	DigitalBinding mouseBinding;
	mouseBinding.action = 7u;
	mouseBinding.key = KeyCode::Escape;
	mouseBinding.button = MouseButton::X1;
	mouseBinding.useMouse = true;

	EXPECT_EQ(mouseBinding.action, 7u);
	EXPECT_EQ(mouseBinding.key, KeyCode::Escape);
	EXPECT_EQ(mouseBinding.button, MouseButton::X1);
	EXPECT_TRUE(mouseBinding.useMouse);

	DigitalBinding copy = keyboardBinding;
	EXPECT_EQ(copy.action, keyboardBinding.action);
	EXPECT_EQ(copy.key, keyboardBinding.key);
	EXPECT_EQ(copy.button, keyboardBinding.button);
	EXPECT_EQ(copy.useMouse, keyboardBinding.useMouse);

	DigitalBinding moved = std::move(mouseBinding);
	EXPECT_EQ(moved.action, 7u);
	EXPECT_EQ(moved.key, KeyCode::Escape);
	EXPECT_EQ(moved.button, MouseButton::X1);
	EXPECT_TRUE(moved.useMouse);
}

TEST(Bindings, test_axis_binding_composes_a_key_pair_into_an_axis)
{
	static_assert(std::is_copy_constructible_v<AxisBinding>);
	static_assert(std::is_copy_assignable_v<AxisBinding>);
	static_assert(std::is_move_constructible_v<AxisBinding>);
	static_assert(std::is_move_assignable_v<AxisBinding>);
	static_assert(std::is_default_constructible_v<AxisBinding>);

	AxisBinding binding;
	binding.action = 11u;
	binding.positive = KeyCode::D;
	binding.negative = KeyCode::A;
	binding.scale = 2.5f;

	EXPECT_EQ(binding.action, 11u);
	EXPECT_EQ(binding.positive, KeyCode::D);
	EXPECT_EQ(binding.negative, KeyCode::A);
	EXPECT_FLOAT_EQ(binding.scale, 2.5f);

	EXPECT_FLOAT_EQ(AxisContribution(binding, false, false), 0.0f);
	EXPECT_FLOAT_EQ(AxisContribution(binding, true, false), 2.5f);
	EXPECT_FLOAT_EQ(AxisContribution(binding, false, true), -2.5f);
	EXPECT_FLOAT_EQ(AxisContribution(binding, true, true), 0.0f);

	binding.positive = KeyCode::Unknown;
	binding.negative = KeyCode::S;
	EXPECT_EQ(binding.positive, KeyCode::Unknown);
	EXPECT_EQ(binding.negative, KeyCode::S);

	AxisBinding copy = binding;
	EXPECT_EQ(copy.action, binding.action);
	EXPECT_EQ(copy.positive, binding.positive);
	EXPECT_EQ(copy.negative, binding.negative);
	EXPECT_FLOAT_EQ(copy.scale, binding.scale);

	AxisBinding moved = std::move(copy);
	EXPECT_EQ(moved.action, 11u);
	EXPECT_EQ(moved.positive, KeyCode::Unknown);
	EXPECT_EQ(moved.negative, KeyCode::S);
	EXPECT_FLOAT_EQ(moved.scale, 2.5f);
}

TEST(Bindings, test_bindings_is_a_mutable_table_of_digital_and_axis_bindings)
{
	static_assert(std::is_final_v<Bindings>);
	static_assert(std::is_copy_constructible_v<Bindings>);
	static_assert(std::is_copy_assignable_v<Bindings>);
	static_assert(std::is_move_constructible_v<Bindings>);
	static_assert(std::is_move_assignable_v<Bindings>);
	static_assert(std::is_default_constructible_v<Bindings>);
	static_assert(noexcept(std::declval<const Bindings&>().DigitalBindings()));
	static_assert(noexcept(std::declval<const Bindings&>().AxisBindings()));
	static_assert(std::is_same_v<
		decltype(std::declval<const Bindings&>().DigitalBindings()),
		std::span<const DigitalBinding>>);
	static_assert(std::is_same_v<
		decltype(std::declval<const Bindings&>().AxisBindings()),
		std::span<const AxisBinding>>);

	Bindings bindings;
	EXPECT_TRUE(bindings.DigitalBindings().empty());
	EXPECT_TRUE(bindings.AxisBindings().empty());

	const ActionId jump = 2u;
	const ActionId fire = 4u;
	const ActionId invalid = ActionState::kInvalidActionId;

	bindings.BindDigital(jump, KeyCode::Space);
	bindings.BindDigital(jump, KeyCode::Enter);
	bindings.BindDigitalMouse(fire, MouseButton::Left);
	bindings.BindDigital(invalid, KeyCode::F1);
	bindings.BindAxis(6u, KeyCode::D, KeyCode::A, 1.0f);
	bindings.BindAxis(6u, KeyCode::Right, KeyCode::Left, 0.5f);
	bindings.BindAxis(invalid, KeyCode::Unknown, KeyCode::Down, 3.0f);

	const auto digital = bindings.DigitalBindings();
	const auto axis = bindings.AxisBindings();

	ASSERT_EQ(digital.size(), 4u);
	EXPECT_EQ(digital[0].action, jump);
	EXPECT_EQ(digital[0].key, KeyCode::Space);
	EXPECT_EQ(digital[0].button, DigitalBinding{}.button);
	EXPECT_FALSE(digital[0].useMouse);
	EXPECT_EQ(digital[1].action, jump);
	EXPECT_EQ(digital[1].key, KeyCode::Enter);
	EXPECT_EQ(digital[1].button, DigitalBinding{}.button);
	EXPECT_FALSE(digital[1].useMouse);
	EXPECT_EQ(digital[2].action, fire);
	EXPECT_EQ(digital[2].key, DigitalBinding{}.key);
	EXPECT_EQ(digital[2].button, MouseButton::Left);
	EXPECT_TRUE(digital[2].useMouse);
	EXPECT_EQ(digital[3].action, invalid);
	EXPECT_EQ(digital[3].key, KeyCode::F1);
	EXPECT_EQ(digital[3].button, DigitalBinding{}.button);
	EXPECT_FALSE(digital[3].useMouse);

	ASSERT_EQ(axis.size(), 3u);
	EXPECT_EQ(axis[0].action, 6u);
	EXPECT_EQ(axis[0].positive, KeyCode::D);
	EXPECT_EQ(axis[0].negative, KeyCode::A);
	EXPECT_FLOAT_EQ(axis[0].scale, 1.0f);
	EXPECT_EQ(axis[1].action, 6u);
	EXPECT_EQ(axis[1].positive, KeyCode::Right);
	EXPECT_EQ(axis[1].negative, KeyCode::Left);
	EXPECT_FLOAT_EQ(axis[1].scale, 0.5f);
	EXPECT_EQ(axis[2].action, invalid);
	EXPECT_EQ(axis[2].positive, KeyCode::Unknown);
	EXPECT_EQ(axis[2].negative, KeyCode::Down);
	EXPECT_FLOAT_EQ(axis[2].scale, 3.0f);

	Bindings copy = bindings;
	ASSERT_EQ(copy.DigitalBindings().size(), digital.size());
	ASSERT_EQ(copy.AxisBindings().size(), axis.size());
	EXPECT_EQ(copy.DigitalBindings()[2].button, MouseButton::Left);
	EXPECT_EQ(copy.AxisBindings()[1].positive, KeyCode::Right);

	Bindings moved = std::move(copy);
	ASSERT_EQ(moved.DigitalBindings().size(), digital.size());
	ASSERT_EQ(moved.AxisBindings().size(), axis.size());
	EXPECT_EQ(moved.DigitalBindings()[3].action, invalid);
	EXPECT_EQ(moved.AxisBindings()[2].negative, KeyCode::Down);

	bindings.Clear();
	EXPECT_TRUE(bindings.DigitalBindings().empty());
	EXPECT_TRUE(bindings.AxisBindings().empty());
}
