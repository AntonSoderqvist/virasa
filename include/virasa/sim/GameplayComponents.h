#ifndef VIRASA_SIM_GAMEPLAYCOMPONENTS_H
#define VIRASA_SIM_GAMEPLAYCOMPONENTS_H

#include "virasa/math/Types.h"

namespace virasa::sim
{

/**
 * @brief Authored local-space angular velocity for the built-in Spin behavior.
 *
 * The vector uses the rotation-vector convention: direction is the local rotation
 * axis and magnitude is radians per second.
 */
struct SpinComponent
{
public:
	/// @brief Local-space angular velocity in radians per second.
	virasa::math::Vec3 angularVelocity = virasa::math::Vec3(0.0f, 0.0f, 0.0f);
};

} // namespace virasa::sim

#endif // VIRASA_SIM_GAMEPLAYCOMPONENTS_H
