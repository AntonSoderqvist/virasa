#ifndef VIRASA_MATH_PROJECTION_H
#define VIRASA_MATH_PROJECTION_H

#include "glm/gtc/matrix_transform.hpp"
#include "virasa/math/Types.h"

namespace virasa::math
{

/**
 * @brief Compute a right-handed look-at view matrix with Z as the world-up axis.
 *
 * Returns a Mat4 view matrix that transforms positions from Z-up right-handed
 * world space into view space. The camera looks toward @p target from @p eye,
 * with Vec3(0, 0, 1) used as the world-up vector.
 *
 * @param eye    Camera position in world space.
 * @param target World-space point the camera looks toward.
 * @return View matrix mapping world space to camera (view) space.
 *
 * @note It is a contract violation for @p eye and @p target to be equal, or
 *       for the vector (target - eye) to be parallel to Vec3(0, 0, 1).
 */
[[nodiscard]] inline Mat4 LookAtRH_ZUp(const Vec3& eye, const Vec3& target)
{
	return glm::lookAt(eye, target, Vec3(0.0f, 0.0f, 1.0f));
}

/**
 * @brief Compute a right-handed perspective projection matrix for Vulkan.
 *
 * Returns a Mat4 perspective projection matrix that maps depth to [0, 1]
 * (zero-to-one, Vulkan convention) and has the Vulkan Y-flip already applied
 * via negation of the [1][1] element. Callers receive a matrix that produces
 * correct upright rendering without any viewport-height manipulation.
 *
 * @param fov_y        Vertical field of view in radians.
 * @param aspect_ratio Width divided by height.
 * @param near_plane   Near clip plane distance (must be positive and < far_plane).
 * @param far_plane    Far clip plane distance (must be positive and > near_plane).
 * @return Perspective projection matrix suitable for Vulkan clip space.
 *
 * @note Requires GLM_FORCE_DEPTH_ZERO_TO_ONE and GLM_FORCE_RADIANS to be
 *       defined project-wide before any GLM include.
 * @note It is a contract violation for near_plane or far_plane to be
 *       non-positive, or for near_plane >= far_plane.
 */
[[nodiscard]] inline Mat4 PerspectiveRH_ZO(
	float fov_y, float aspect_ratio, float near_plane, float far_plane)
{
	Mat4 projection = glm::perspectiveRH_ZO(fov_y, aspect_ratio, near_plane, far_plane);
	projection[1][1] *= -1.0f;
	return projection;
}

/**
 * @brief Compute a right-handed orthographic projection matrix for Vulkan.
 *
 * Returns a Mat4 orthographic projection matrix that maps depth to [0, 1]
 * (zero-to-one, Vulkan convention) and has the Vulkan Y-flip already applied
 * via negation of the [1][1] element. Callers receive a matrix that produces
 * correct upright rendering without any viewport-height manipulation.
 *
 * @param left        Left clip boundary in view space.
 * @param right       Right clip boundary in view space.
 * @param bottom      Bottom clip boundary in view space.
 * @param top         Top clip boundary in view space.
 * @param near_plane  Near depth clip boundary.
 * @param far_plane   Far depth clip boundary.
 * @return Orthographic projection matrix suitable for Vulkan clip space.
 *
 * @note It is a contract violation for left == right, bottom == top, or
 *       near_plane == far_plane, as each produces a degenerate matrix.
 */
[[nodiscard]] inline Mat4 OrthoRH_ZO(
	float left, float right, float bottom, float top, float near_plane, float far_plane)
{
	Mat4 projection = glm::orthoRH_ZO(left, right, bottom, top, near_plane, far_plane);
	projection[1][1] *= -1.0f;
	return projection;
}

} // namespace virasa::math

#endif // VIRASA_MATH_PROJECTION_H
