#ifndef VIRASA_MATH_TYPES_H
#define VIRASA_MATH_TYPES_H

// GLM_FORCE_DEPTH_ZERO_TO_ONE and GLM_FORCE_RADIANS must be defined project-wide
// (e.g. via CMake target_compile_definitions) before any GLM header is included.
// Do NOT define them here; they must be consistent across all translation units.

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat3x3.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/quaternion.hpp"

namespace virasa::math
{

/**
 * @brief Two-component single-precision floating-point vector (glm::vec2).
 */
using Vec2 = glm::vec2;

/**
 * @brief Three-component single-precision floating-point vector (glm::vec3).
 */
using Vec3 = glm::vec3;

/**
 * @brief Four-component single-precision floating-point vector (glm::vec4).
 */
using Vec4 = glm::vec4;

/**
 * @brief 3x3 column-major single-precision floating-point matrix (glm::mat3).
 */
using Mat3 = glm::mat3;

/**
 * @brief 4x4 column-major single-precision floating-point matrix (glm::mat4).
 *
 * When passed as a push constant or uniform, laid out as 16 contiguous floats
 * in column-major order, matching the mat4 layout expected by GLSL.
 */
using Mat4 = glm::mat4;

/**
 * @brief Single-precision quaternion (glm::quat).
 *
 * Components are stored in GLM's (w, x, y, z) order.
 */
using Quat = glm::quat;

} // namespace virasa::math

#endif // VIRASA_MATH_TYPES_H
