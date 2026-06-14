#include <gtest/gtest.h>

#include "virasa/sim/GameplayComponents.h"

#include "virasa/math/Types.h"

#include <type_traits>

using namespace virasa::math;
using namespace virasa::sim;

TEST(GameplayComponents, test_spin_component_holds_local_angular_velocity)
{
	static_assert(std::is_copy_constructible_v<SpinComponent>);
	static_assert(std::is_copy_assignable_v<SpinComponent>);
	static_assert(std::is_move_constructible_v<SpinComponent>);
	static_assert(std::is_move_assignable_v<SpinComponent>);
	static_assert(std::is_default_constructible_v<SpinComponent>);
	static_assert(std::is_trivially_destructible_v<SpinComponent>);
	static_assert(std::is_trivially_copyable_v<SpinComponent>);

	SpinComponent spin;
	EXPECT_FLOAT_EQ(spin.angularVelocity.x, 0.0f);
	EXPECT_FLOAT_EQ(spin.angularVelocity.y, 0.0f);
	EXPECT_FLOAT_EQ(spin.angularVelocity.z, 0.0f);

	spin.angularVelocity = Vec3(0.0f, 0.0f, 1.0f);

	SpinComponent copied = spin;
	EXPECT_FLOAT_EQ(copied.angularVelocity.x, 0.0f);
	EXPECT_FLOAT_EQ(copied.angularVelocity.y, 0.0f);
	EXPECT_FLOAT_EQ(copied.angularVelocity.z, 1.0f);
}
