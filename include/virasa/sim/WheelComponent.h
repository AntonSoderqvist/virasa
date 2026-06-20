// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#ifndef VIRASA_SIM_WHEELCOMPONENT_H
#define VIRASA_SIM_WHEELCOMPONENT_H

namespace virasa::sim
{

/**
 * @brief Authored role and radius tuning for one vehicle wheel entity.
 *
 * WheelComponent carries authoring data only: it owns no runtime physics
 * handles, simulated state, or entity references.
 */
struct WheelComponent
{
public:
	/// @brief True when this wheel turns with the steering input.
	bool steered = false;

	/// @brief True when this wheel receives engine drive force.
	bool driven = false;

	/// @brief Contact radius multiplier applied on top of the wheel entity size.
	float radiusFactor = 1.0f;
};

} // namespace virasa::sim

#endif // VIRASA_SIM_WHEELCOMPONENT_H
