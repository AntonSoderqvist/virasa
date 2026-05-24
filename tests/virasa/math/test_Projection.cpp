#include <gtest/gtest.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "virasa/math/Projection.h"
#include "virasa/math/Types.h"

using namespace virasa::math;

// ---------------------------------------------------------------------------
// LookAtRH_ZUp
// ---------------------------------------------------------------------------

TEST(Projection, test_look_at_rh_z_up_produces_view_matrix)
{
	// A camera sitting on the positive X axis looking toward the origin.
	// With Z-up RHC the forward vector is (-1, 0, 0), which is not parallel
	// to world-up (0, 0, 1), so the call is well-defined.
	const Vec3 eye(5.0f, 0.0f, 0.0f);
	const Vec3 target(0.0f, 0.0f, 0.0f);

	const Mat4 result = LookAtRH_ZUp(eye, target);

	// The reference is the GLM call with the same arguments and world-up (0,0,1).
	const Mat4 expected = glm::lookAt(eye, target, Vec3(0.0f, 0.0f, 1.0f));

	for (int col = 0; col < 4; ++col)
	{
		for (int row = 0; row < 4; ++row)
		{
			EXPECT_NEAR(result[col][row], expected[col][row], 1e-5f)
				<< "Mismatch at col=" << col << " row=" << row;
		}
	}

	// Sanity-check: the eye position transformed by the view matrix should
	// land at the origin of view space (the camera is at view-space origin).
	const glm::vec4 eyeInView = result * glm::vec4(eye, 1.0f);
	EXPECT_NEAR(eyeInView.x, 0.0f, 1e-5f);
	EXPECT_NEAR(eyeInView.y, 0.0f, 1e-5f);
	EXPECT_NEAR(eyeInView.z, 0.0f, 1e-5f);

	// The target should be in front of the camera along the -Z view-space axis.
	const glm::vec4 targetInView = result * glm::vec4(target, 1.0f);
	EXPECT_LT(targetInView.z, 0.0f);
}

// ---------------------------------------------------------------------------
// PerspectiveRH_ZO
// ---------------------------------------------------------------------------

TEST(Projection, test_perspective_rh_zo_produces_vulkan_projection_matrix)
{
	const float fovY = glm::radians(60.0f);
	const float aspect = 16.0f / 9.0f;
	const float nearPlane = 0.1f;
	const float farPlane = 1000.0f;

	const Mat4 result = PerspectiveRH_ZO(fovY, aspect, nearPlane, farPlane);

	// Build the reference: GLM perspective with ZO + Y-flip on [1][1].
	Mat4 expected = glm::perspectiveRH_ZO(fovY, aspect, nearPlane, farPlane);
	expected[1][1] *= -1.0f;

	for (int col = 0; col < 4; ++col)
	{
		for (int row = 0; row < 4; ++row)
		{
			EXPECT_NEAR(result[col][row], expected[col][row], 1e-5f)
				<< "Mismatch at col=" << col << " row=" << row;
		}
	}

	// Depth mapping: a point at near_plane in view space (z = -near_plane in
	// RHC view space) should map to NDC depth 0, and a point at far_plane
	// should map to NDC depth 1.
	//
	// In clip space: depth_ndc = clip_z / clip_w.
	// View-space point at the near plane: (0, 0, -nearPlane, 1).
	const glm::vec4 nearPoint(0.0f, 0.0f, -nearPlane, 1.0f);
	const glm::vec4 nearClip = result * nearPoint;
	EXPECT_NEAR(nearClip.z / nearClip.w, 0.0f, 1e-5f);

	// View-space point at the far plane: (0, 0, -farPlane, 1).
	const glm::vec4 farPoint(0.0f, 0.0f, -farPlane, 1.0f);
	const glm::vec4 farClip = result * farPoint;
	EXPECT_NEAR(farClip.z / farClip.w, 1.0f, 1e-4f);

	// Y-flip: [1][1] must be negative so that Vulkan clip-space Y points down.
	EXPECT_LT(result[1][1], 0.0f);
}

// ---------------------------------------------------------------------------
// OrthoRH_ZO
// ---------------------------------------------------------------------------

TEST(Projection, test_ortho_rh_zo_produces_vulkan_orthographic_projection_matrix)
{
	const float left = -10.0f;
	const float right = 10.0f;
	const float bottom = -5.0f;
	const float top = 5.0f;
	const float nearPlane = 0.0f;
	const float farPlane = 100.0f;

	const Mat4 result = OrthoRH_ZO(left, right, bottom, top, nearPlane, farPlane);

	// Build the reference: GLM ortho with ZO + Y-flip on [1][1].
	Mat4 expected = glm::orthoRH_ZO(left, right, bottom, top, nearPlane, farPlane);
	expected[1][1] *= -1.0f;

	for (int col = 0; col < 4; ++col)
	{
		for (int row = 0; row < 4; ++row)
		{
			EXPECT_NEAR(result[col][row], expected[col][row], 1e-5f)
				<< "Mismatch at col=" << col << " row=" << row;
		}
	}

	// Depth mapping: near_plane maps to NDC depth 0.
	// In an ortho matrix the w component stays 1, so depth_ndc == clip_z.
	const glm::vec4 nearPoint(0.0f, 0.0f, -nearPlane, 1.0f);
	const glm::vec4 nearClip = result * nearPoint;
	EXPECT_NEAR(nearClip.z / nearClip.w, 0.0f, 1e-5f);

	// far_plane maps to NDC depth 1.
	const glm::vec4 farPoint(0.0f, 0.0f, -farPlane, 1.0f);
	const glm::vec4 farClip = result * farPoint;
	EXPECT_NEAR(farClip.z / farClip.w, 1.0f, 1e-5f);

	// Y-flip: [1][1] must be negative.
	EXPECT_LT(result[1][1], 0.0f);

	// Horizontal boundary: the left edge in view space maps to NDC x = -1.
	const glm::vec4 leftPoint(left, 0.0f, 0.0f, 1.0f);
	const glm::vec4 leftClip = result * leftPoint;
	EXPECT_NEAR(leftClip.x / leftClip.w, -1.0f, 1e-5f);

	// The right edge maps to NDC x = +1.
	const glm::vec4 rightPoint(right, 0.0f, 0.0f, 1.0f);
	const glm::vec4 rightClip = result * rightPoint;
	EXPECT_NEAR(rightClip.x / rightClip.w, 1.0f, 1e-5f);
}
