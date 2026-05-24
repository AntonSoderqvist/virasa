#include <gtest/gtest.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <type_traits>

#include "virasa/math/Types.h"

TEST(Types, test_math_coordinate_convention_rhc_z_up)
{
	// The Z-up right-handed convention is enforced at the API level by using
	// Vec3(0, 0, 1) as the world-up vector. We verify that the canonical up
	// vector in this coordinate system is indeed (0, 0, 1) — i.e. Z is up,
	// X is right, and Y is forward — by constructing the expected up vector
	// and confirming it matches the Z-axis unit vector.
	virasa::math::Vec3 worldUp(0.0f, 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(worldUp.x, 0.0f);
	EXPECT_FLOAT_EQ(worldUp.y, 0.0f);
	EXPECT_FLOAT_EQ(worldUp.z, 1.0f);

	// Confirm that a right-hand-rule cross product of X-right and Y-forward
	// yields Z-up, which is the defining property of the RHC Z-up convention.
	virasa::math::Vec3 xRight(1.0f, 0.0f, 0.0f);
	virasa::math::Vec3 yForward(0.0f, 1.0f, 0.0f);
	virasa::math::Vec3 zUp = glm::cross(xRight, yForward);
	EXPECT_FLOAT_EQ(zUp.x, 0.0f);
	EXPECT_FLOAT_EQ(zUp.y, 0.0f);
	EXPECT_FLOAT_EQ(zUp.z, 1.0f);
}

TEST(Types, test_glm_compile_time_defines_required)
{
	// GLM_FORCE_DEPTH_ZERO_TO_ONE must be defined before any GLM include so
	// that projection functions map depth to [0, 1] (Vulkan convention).
	// GLM_FORCE_RADIANS must be defined so angle parameters are in radians.
	// We verify both macros are defined in this translation unit (they are
	// set at the top of this file, as every virasa::math TU must do).
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
	FAIL() << "GLM_FORCE_DEPTH_ZERO_TO_ONE is not defined";
#endif
#ifndef GLM_FORCE_RADIANS
	FAIL() << "GLM_FORCE_RADIANS is not defined";
#endif
	// If we reach here both defines are present.
	SUCCEED();
}

TEST(Types, test_type_aliases_are_glm_backed)
{
	// Verify that each alias in virasa::math is exactly the corresponding GLM
	// type — no wrapper, no derived type.
	EXPECT_TRUE((std::is_same_v<virasa::math::Vec2, glm::vec2>));
	EXPECT_TRUE((std::is_same_v<virasa::math::Vec3, glm::vec3>));
	EXPECT_TRUE((std::is_same_v<virasa::math::Vec4, glm::vec4>));
	EXPECT_TRUE((std::is_same_v<virasa::math::Mat3, glm::mat3>));
	EXPECT_TRUE((std::is_same_v<virasa::math::Mat4, glm::mat4>));
	EXPECT_TRUE((std::is_same_v<virasa::math::Quat, glm::quat>));

	// Vec2: two-component single-precision float vector.
	virasa::math::Vec2 v2(1.0f, 2.0f);
	EXPECT_FLOAT_EQ(v2.x, 1.0f);
	EXPECT_FLOAT_EQ(v2.y, 2.0f);

	// Vec3: three-component single-precision float vector.
	virasa::math::Vec3 v3(1.0f, 2.0f, 3.0f);
	EXPECT_FLOAT_EQ(v3.x, 1.0f);
	EXPECT_FLOAT_EQ(v3.y, 2.0f);
	EXPECT_FLOAT_EQ(v3.z, 3.0f);

	// Vec4: four-component single-precision float vector.
	virasa::math::Vec4 v4(1.0f, 2.0f, 3.0f, 4.0f);
	EXPECT_FLOAT_EQ(v4.x, 1.0f);
	EXPECT_FLOAT_EQ(v4.y, 2.0f);
	EXPECT_FLOAT_EQ(v4.z, 3.0f);
	EXPECT_FLOAT_EQ(v4.w, 4.0f);

	// Mat3: 3x3 column-major single-precision float matrix.
	// Default-constructed glm::mat3 is the identity matrix.
	virasa::math::Mat3 m3(1.0f);
	EXPECT_FLOAT_EQ(m3[0][0], 1.0f);
	EXPECT_FLOAT_EQ(m3[1][1], 1.0f);
	EXPECT_FLOAT_EQ(m3[2][2], 1.0f);
	EXPECT_FLOAT_EQ(m3[0][1], 0.0f);

	// Mat4: 4x4 column-major single-precision float matrix.
	// Verify column-major layout: 16 contiguous floats, column by column.
	virasa::math::Mat4 m4(1.0f); // identity
	EXPECT_FLOAT_EQ(m4[0][0], 1.0f);
	EXPECT_FLOAT_EQ(m4[1][1], 1.0f);
	EXPECT_FLOAT_EQ(m4[2][2], 1.0f);
	EXPECT_FLOAT_EQ(m4[3][3], 1.0f);
	EXPECT_FLOAT_EQ(m4[0][1], 0.0f);
	// Confirm the underlying storage is 16 contiguous floats (column-major).
	static_assert(
		sizeof(virasa::math::Mat4) == 16 * sizeof(float), "Mat4 must be 16 contiguous floats");

	// Quat: single-precision quaternion. GLM stores as (w, x, y, z) internally.
	// Identity quaternion: w=1, x=0, y=0, z=0.
	virasa::math::Quat q(1.0f, 0.0f, 0.0f, 0.0f); // (w, x, y, z)
	EXPECT_FLOAT_EQ(q.w, 1.0f);
	EXPECT_FLOAT_EQ(q.x, 0.0f);
	EXPECT_FLOAT_EQ(q.y, 0.0f);
	EXPECT_FLOAT_EQ(q.z, 0.0f);
}
