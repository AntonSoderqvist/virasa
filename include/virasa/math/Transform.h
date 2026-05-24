#ifndef VIRASA_MATH_TRANSFORM_H
#define VIRASA_MATH_TRANSFORM_H

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "virasa/math/Types.h"

namespace virasa::math
{

/**
 * @brief Holds the three components of a TRS (translate-rotate-scale) local-space transform.
 *
 * Transform stores a position (translation), orientation (rotation as a unit quaternion),
 * and per-axis scale. All values are expressed in Z-up right-handed coordinate space.
 * A default-constructed Transform represents the identity transform.
 */
struct Transform
{
	public:
	/// @brief The object's position in world space.
	Vec3 translation = Vec3(0.0f, 0.0f, 0.0f);

	/// @brief The object's orientation as a unit quaternion in (w, x, y, z) storage order.
	Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);

	/// @brief Per-axis scale factors.
	Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);

	/**
	 * @brief Compute the full TRS model matrix (T * R * S).
	 *
	 * Returns a column-major Mat4 suitable for use as a model matrix in an MVP product.
	 * Scale is applied first, then rotation, then translation.
	 * The rotation quaternion is assumed to be a unit quaternion.
	 *
	 * @return Mat4 representing this transform.
	 */
	[[nodiscard]] Mat4 ToMatrix() const noexcept
	{
		Mat4 t = glm::translate(Mat4(1.0f), translation);
		Mat4 r = glm::mat4_cast(rotation);
		Mat4 s = glm::scale(Mat4(1.0f), scale);
		return t * r * s;
	}

	/**
	 * @brief Returns the local +Y axis (forward direction) rotated into world space.
	 *
	 * In the Z-up RHC convention, +Y is the canonical forward direction.
	 * An identity-rotation Transform returns Vec3(0, 1, 0).
	 *
	 * @return Normalized world-space forward vector.
	 */
	[[nodiscard]] Vec3 Forward() const noexcept
	{
		return glm::normalize(rotation * Vec3(0.0f, 1.0f, 0.0f));
	}

	/**
	 * @brief Returns the local +X axis (right direction) rotated into world space.
	 *
	 * An identity-rotation Transform returns Vec3(1, 0, 0).
	 *
	 * @return Normalized world-space right vector.
	 */
	[[nodiscard]] Vec3 Right() const noexcept
	{
		return glm::normalize(rotation * Vec3(1.0f, 0.0f, 0.0f));
	}

	/**
	 * @brief Returns the local +Z axis (up direction) rotated into world space.
	 *
	 * An identity-rotation Transform returns Vec3(0, 0, 1).
	 *
	 * @return Normalized world-space up vector.
	 */
	[[nodiscard]] Vec3 Up() const noexcept
	{
		return glm::normalize(rotation * Vec3(0.0f, 0.0f, 1.0f));
	}

	/**
	 * @brief Returns a default-constructed (identity) Transform.
	 *
	 * Equivalent to a default constructor call: translation (0,0,0),
	 * rotation Quat(1,0,0,0), scale (1,1,1).
	 *
	 * @return Identity Transform.
	 */
	[[nodiscard]] static Transform Identity() noexcept
	{
		return Transform{};
	}
};

} // namespace virasa::math

#endif // VIRASA_MATH_TRANSFORM_H
