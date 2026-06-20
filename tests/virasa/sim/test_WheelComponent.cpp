#include <gtest/gtest.h>

#include "virasa/sim/WheelComponent.h"

#include <type_traits>

using namespace virasa::sim;

TEST(WheelComponent, test_wheel_component_holds_authored_wheel_role)
{
	static_assert(std::is_copy_constructible_v<WheelComponent>);
	static_assert(std::is_copy_assignable_v<WheelComponent>);
	static_assert(std::is_move_constructible_v<WheelComponent>);
	static_assert(std::is_move_assignable_v<WheelComponent>);
	static_assert(std::is_default_constructible_v<WheelComponent>);
	static_assert(std::is_trivially_destructible_v<WheelComponent>);
	static_assert(std::is_trivially_copyable_v<WheelComponent>);

	WheelComponent wheel;
	EXPECT_EQ(wheel.steered, false);
	EXPECT_EQ(wheel.driven, false);
	EXPECT_FLOAT_EQ(wheel.radiusFactor, 1.0f);

	wheel.steered = true;
	wheel.driven = true;
	wheel.radiusFactor = 0.85f;

	WheelComponent copied = wheel;
	EXPECT_EQ(copied.steered, true);
	EXPECT_EQ(copied.driven, true);
	EXPECT_FLOAT_EQ(copied.radiusFactor, 0.85f);

	WheelComponent assigned;
	assigned = copied;
	EXPECT_EQ(assigned.steered, true);
	EXPECT_EQ(assigned.driven, true);
	EXPECT_FLOAT_EQ(assigned.radiusFactor, 0.85f);
}
