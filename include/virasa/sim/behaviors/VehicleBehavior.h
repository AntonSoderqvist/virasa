// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#ifndef VIRASA_SIM_BEHAVIORS_VEHICLEBEHAVIOR_H
#define VIRASA_SIM_BEHAVIORS_VEHICLEBEHAVIOR_H

#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/Types.h"
#include "virasa/math/Types.h"

#include <cstdint>
#include <unordered_map>

namespace virasa::ecs
{
class CommandBuffer;
class World;
}

namespace virasa::sim
{
struct TickContext;
}

namespace virasa::sim::behaviors
{

/**
 * @brief Fixed-step behavior that applies authored vehicle wheel forces.
 */
class VehicleBehavior final : public virasa::ecs::Behavior
{
public:
	VehicleBehavior() = default;
	VehicleBehavior(const VehicleBehavior&) = delete;
	VehicleBehavior& operator=(const VehicleBehavior&) = delete;
	VehicleBehavior(VehicleBehavior&&) = delete;
	VehicleBehavior& operator=(VehicleBehavior&&) = delete;

	/// @brief Virtual destructor.
	virtual ~VehicleBehavior() noexcept = default;

	/**
	 * @brief Return the stable behavior name.
	 * @return Null-terminated "Vehicle" string.
	 */
	[[nodiscard]] const char* Name() const noexcept override;

	/**
	 * @brief Accumulate suspension, drive, brake, and grip forces for vehicle bodies.
	 * @param world World containing Vehicle components and shared physics/input resources.
	 * @param tick Fixed-step timing data.
	 * @param commands Deferred command buffer, left untouched by this behavior.
	 */
	void Step(
		virasa::ecs::World& world,
		const virasa::sim::TickContext& tick,
		virasa::ecs::CommandBuffer& commands) override;

private:
	std::unordered_map<uint32_t, float> _wheelRollAngles;
	std::unordered_map<uint32_t, virasa::math::Vec3> _wheelRestMounts;
};

} // namespace virasa::sim::behaviors

#endif // VIRASA_SIM_BEHAVIORS_VEHICLEBEHAVIOR_H
