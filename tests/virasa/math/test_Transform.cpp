#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <gtest/gtest.h>

#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"

using namespace virasa::math;

namespace
{

constexpr float kEpsilon = 1e-5f;

bool Vec3Near(const Vec3& a, const Vec3& b, float eps = kEpsilon)
{
	return std::abs(a.x - b.x) < eps && std::abs(a.y - b.y) < eps && std::abs(a.z - b.z) < eps;
}

bool Mat4Near(const Mat4& a, const Mat4& b, float eps = kEpsilon)
{
	for (int col = 0; col < 4; ++col)
		for (int row = 0; row < 4; ++row)
			if (std::abs(a[col][row] - b[col][row]) >= eps)
				return false;
	return true;
}

} // namespace

TEST(Transform, test_transform_holds_trs_components)
{
	// Default construction produces the canonical zero-translation, identity-rotation, unit-scale
	// state.
	Transform t;

	EXPECT_FLOAT_EQ(t.translation.x, 0.0f);
	EXPECT_FLOAT_EQ(t.translation.y, 0.0f);
	EXPECT_FLOAT_EQ(t.translation.z, 0.0f);

	// GLM quat stores (w, x, y, z); identity is w=1, x=y=z=0.
	EXPECT_FLOAT_EQ(t.rotation.w, 1.0f);
	EXPECT_FLOAT_EQ(t.rotation.x, 0.0f);
	EXPECT_FLOAT_EQ(t.rotation.y, 0.0f);
	EXPECT_FLOAT_EQ(t.rotation.z, 0.0f);

	EXPECT_FLOAT_EQ(t.scale.x, 1.0f);
	EXPECT_FLOAT_EQ(t.scale.y, 1.0f);
	EXPECT_FLOAT_EQ(t.scale.z, 1.0f);

	// Verify copyable and movable.
	Transform copy = t;
	EXPECT_FLOAT_EQ(copy.translation.x, 0.0f);
	EXPECT_FLOAT_EQ(copy.rotation.w, 1.0f);
	EXPECT_FLOAT_EQ(copy.scale.x, 1.0f);

	Transform moved = std::move(copy);
	EXPECT_FLOAT_EQ(moved.translation.x, 0.0f);
	EXPECT_FLOAT_EQ(moved.rotation.w, 1.0f);
	EXPECT_FLOAT_EQ(moved.scale.x, 1.0f);

	// Verify members are publicly assignable.
	t.translation = Vec3(1.0f, 2.0f, 3.0f);
	t.rotation = Quat(0.0f, 1.0f, 0.0f, 0.0f);
	t.scale = Vec3(2.0f, 3.0f, 4.0f);

	EXPECT_FLOAT_EQ(t.translation.x, 1.0f);
	EXPECT_FLOAT_EQ(t.translation.y, 2.0f);
	EXPECT_FLOAT_EQ(t.translation.z, 3.0f);
	EXPECT_FLOAT_EQ(t.rotation.w, 0.0f);
	EXPECT_FLOAT_EQ(t.rotation.x, 1.0f);
	EXPECT_FLOAT_EQ(t.scale.x, 2.0f);
	EXPECT_FLOAT_EQ(t.scale.y, 3.0f);
	EXPECT_FLOAT_EQ(t.scale.z, 4.0f);
}

TEST(Transform, test_transform_to_matrix_produces_trs_mat4)
{
	// Build a known TRS and compare against a manually computed T*R*S.
	const Vec3 translation(3.0f, -1.0f, 2.0f);
	// 90-degree rotation around Z axis (in Z-up RHC space).
	const Quat rotation = glm::angleAxis(glm::radians(90.0f), Vec3(0.0f, 0.0f, 1.0f));
	const Vec3 scale(2.0f, 3.0f, 0.5f);

	Transform t;
	t.translation = translation;
	t.rotation = rotation;
	t.scale = scale;

	const Mat4 result = t.ToMatrix();

	// Manually compute T * R * S.
	const Mat4 T = glm::translate(Mat4(1.0f), translation);
	const Mat4 R = glm::mat4_cast(rotation);
	const Mat4 S = glm::scale(Mat4(1.0f), scale);
	const Mat4 expected = T * R * S;

	EXPECT_TRUE(Mat4Near(result, expected))
		<< "ToMatrix() did not produce the expected T*R*S matrix.";

	// Identity transform must produce the identity matrix.
	Transform identity;
	const Mat4 identityMat = identity.ToMatrix();
	EXPECT_TRUE(Mat4Near(identityMat, Mat4(1.0f)))
		<< "Default-constructed Transform::ToMatrix() should be the identity matrix.";

	// Translation-only transform: the last column should carry the translation.
	Transform transOnly;
	transOnly.translation = Vec3(5.0f, 6.0f, 7.0f);
	const Mat4 transOnlyMat = transOnly.ToMatrix();
	EXPECT_NEAR(transOnlyMat[3][0], 5.0f, kEpsilon);
	EXPECT_NEAR(transOnlyMat[3][1], 6.0f, kEpsilon);
	EXPECT_NEAR(transOnlyMat[3][2], 7.0f, kEpsilon);

	// Scale-only transform: diagonal should reflect scale.
	Transform scaleOnly;
	scaleOnly.scale = Vec3(2.0f, 3.0f, 4.0f);
	const Mat4 scaleOnlyMat = scaleOnly.ToMatrix();
	EXPECT_NEAR(scaleOnlyMat[0][0], 2.0f, kEpsilon);
	EXPECT_NEAR(scaleOnlyMat[1][1], 3.0f, kEpsilon);
	EXPECT_NEAR(scaleOnlyMat[2][2], 4.0f, kEpsilon);
}

TEST(Transform, test_transform_forward_right_up_are_local_axes_in_world_space)
{
	// Identity rotation: axes must match the canonical Z-up RHC directions.
	Transform t;

	const Vec3 forward = t.Forward();
	const Vec3 right = t.Right();
	const Vec3 up = t.Up();

	EXPECT_TRUE(Vec3Near(forward, Vec3(0.0f, 1.0f, 0.0f)))
		<< "Identity Forward() should be +Y (canonical forward in Z-up RHC).";
	EXPECT_TRUE(Vec3Near(right, Vec3(1.0f, 0.0f, 0.0f))) << "Identity Right() should be +X.";
	EXPECT_TRUE(Vec3Near(up, Vec3(0.0f, 0.0f, 1.0f)))
		<< "Identity Up() should be +Z (world up in Z-up RHC).";

	// Rotate 90 degrees around Z: +Y (forward) becomes -X, +X (right) becomes +Y.
	t.rotation = glm::angleAxis(glm::radians(90.0f), Vec3(0.0f, 0.0f, 1.0f));

	const Vec3 rotatedForward = t.Forward();
	const Vec3 rotatedRight = t.Right();
	const Vec3 rotatedUp = t.Up();

	EXPECT_TRUE(Vec3Near(rotatedForward, Vec3(-1.0f, 0.0f, 0.0f)))
		<< "After 90-deg Z rotation, Forward() should be -X.";
	EXPECT_TRUE(Vec3Near(rotatedRight, Vec3(0.0f, 1.0f, 0.0f)))
		<< "After 90-deg Z rotation, Right() should be +Y.";
	// Up (+Z) is unchanged by a rotation around Z.
	EXPECT_TRUE(Vec3Near(rotatedUp, Vec3(0.0f, 0.0f, 1.0f)))
		<< "After 90-deg Z rotation, Up() should still be +Z.";

	// Translation and scale must not affect the direction vectors.
	t.translation = Vec3(100.0f, 200.0f, 300.0f);
	t.scale = Vec3(5.0f, 5.0f, 5.0f);
	t.rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f); // back to identity

	EXPECT_TRUE(Vec3Near(t.Forward(), Vec3(0.0f, 1.0f, 0.0f)))
		<< "Forward() must not be affected by translation or scale.";
	EXPECT_TRUE(Vec3Near(t.Right(), Vec3(1.0f, 0.0f, 0.0f)))
		<< "Right() must not be affected by translation or scale.";
	EXPECT_TRUE(Vec3Near(t.Up(), Vec3(0.0f, 0.0f, 1.0f)))
		<< "Up() must not be affected by translation or scale.";

	// Returned vectors must be unit length.
	const Quat arbitraryRot =
		glm::angleAxis(glm::radians(37.0f), glm::normalize(Vec3(1.0f, 1.0f, 1.0f)));
	t.rotation = arbitraryRot;
	EXPECT_NEAR(glm::length(t.Forward()), 1.0f, kEpsilon);
	EXPECT_NEAR(glm::length(t.Right()), 1.0f, kEpsilon);
	EXPECT_NEAR(glm::length(t.Up()), 1.0f, kEpsilon);
}

TEST(Transform, test_transform_identity_returns_default_transform)
{
	const Transform id = Transform::Identity();
	const Transform def;

	EXPECT_FLOAT_EQ(id.translation.x, def.translation.x);
	EXPECT_FLOAT_EQ(id.translation.y, def.translation.y);
	EXPECT_FLOAT_EQ(id.translation.z, def.translation.z);

	EXPECT_FLOAT_EQ(id.rotation.w, def.rotation.w);
	EXPECT_FLOAT_EQ(id.rotation.x, def.rotation.x);
	EXPECT_FLOAT_EQ(id.rotation.y, def.rotation.y);
	EXPECT_FLOAT_EQ(id.rotation.z, def.rotation.z);

	EXPECT_FLOAT_EQ(id.scale.x, def.scale.x);
	EXPECT_FLOAT_EQ(id.scale.y, def.scale.y);
	EXPECT_FLOAT_EQ(id.scale.z, def.scale.z);

	// Explicit canonical values.
	EXPECT_FLOAT_EQ(id.translation.x, 0.0f);
	EXPECT_FLOAT_EQ(id.translation.y, 0.0f);
	EXPECT_FLOAT_EQ(id.translation.z, 0.0f);
	EXPECT_FLOAT_EQ(id.rotation.w, 1.0f);
	EXPECT_FLOAT_EQ(id.rotation.x, 0.0f);
	EXPECT_FLOAT_EQ(id.rotation.y, 0.0f);
	EXPECT_FLOAT_EQ(id.rotation.z, 0.0f);
	EXPECT_FLOAT_EQ(id.scale.x, 1.0f);
	EXPECT_FLOAT_EQ(id.scale.y, 1.0f);
	EXPECT_FLOAT_EQ(id.scale.z, 1.0f);
}
