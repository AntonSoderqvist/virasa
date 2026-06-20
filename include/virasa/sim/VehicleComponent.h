// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#ifndef VIRASA_SIM_VEHICLECOMPONENT_H
#define VIRASA_SIM_VEHICLECOMPONENT_H

#include "virasa/ecs/Types.h"
#include "virasa/input/Actions.h"

namespace virasa::sim
{

/**
 * @brief Entity references for the wheels attached to a vehicle chassis.
 *
 * VehicleWheels stores optional wheel entities in chassis-relative positions.
 */
struct VehicleWheels
{
public:
	/// @brief Front-right wheel entity.
	virasa::ecs::Entity frontRight = virasa::ecs::Entity::Invalid();

	/// @brief Front-left wheel entity.
	virasa::ecs::Entity frontLeft = virasa::ecs::Entity::Invalid();

	/// @brief Back-right wheel entity.
	virasa::ecs::Entity backRight = virasa::ecs::Entity::Invalid();

	/// @brief Back-left wheel entity.
	virasa::ecs::Entity backLeft = virasa::ecs::Entity::Invalid();
};

/**
 * @brief Authored parameters for a four-wheel vehicle chassis.
 *
 * VehicleComponent carries gameplay authoring data and wheel entity references:
 * it owns no runtime physics handles or simulated state.
 */
struct VehicleComponent
{
public:
	/// @brief Wheel entities attached to this vehicle.
	virasa::sim::VehicleWheels wheels;

	/// @brief Rest length in metres of each suspension strut.
	float suspensionRestLength = 0.6f;

	/// @brief Suspension spring constant in newtons per metre.
	float suspensionStiffness = 30000.0f;

	/// @brief Suspension damping coefficient in newton-seconds per metre.
	float suspensionDamping = 4000.0f;

	/// @brief Steering angle at full steer input in radians.
	float maxSteerAngle = 0.5235987755982988f;

	/// @brief Total drive force in newtons at full throttle.
	float enginePower = 15000.0f;

	/// @brief Total braking force in newtons.
	float brakeForce = 10000.0f;

	/// @brief Lateral grip force scale resisting sideways sliding.
	float tireGrip = 8000.0f;

	/// @brief Input action id for forward and reverse throttle.
	virasa::input::ActionId throttleAction = static_cast<virasa::input::ActionId>(0);

	/// @brief Input action id for left and right steering.
	virasa::input::ActionId steerAction = static_cast<virasa::input::ActionId>(1);

	/// @brief Input action id for braking.
	virasa::input::ActionId brakeAction = static_cast<virasa::input::ActionId>(2);

	/// @brief Input action id for the handbrake.
	virasa::input::ActionId handbrakeAction = static_cast<virasa::input::ActionId>(3);
};

} // namespace virasa::sim

#endif // VIRASA_SIM_VEHICLECOMPONENT_H
