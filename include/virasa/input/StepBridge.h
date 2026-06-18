// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#ifndef VIRASA_INPUT_STEPBRIDGE_H
#define VIRASA_INPUT_STEPBRIDGE_H

#include "virasa/input/Actions.h"

namespace virasa::ecs
{

class World;

} // namespace virasa::ecs

namespace virasa::input
{

/**
 * @brief Bridges frame-resolved input into fixed simulation steps.
 *
 * StepBridge owns the per-step ActionState storage registered as a borrowed
 * World resource. It latches one frame snapshot and republishes it for each
 * fixed step, delivering button edges only to the first step of the frame.
 */
class StepBridge final
{
	public:
	/// @brief Default-constructs a bridge with neutral frame and step snapshots.
	StepBridge() = default;

	StepBridge(const virasa::input::StepBridge&) = delete;
	virasa::input::StepBridge& operator=(const virasa::input::StepBridge&) = delete;
	StepBridge(virasa::input::StepBridge&&) = delete;
	virasa::input::StepBridge& operator=(virasa::input::StepBridge&&) = delete;

	/**
	 * @brief Latches the resolved input snapshot for the current rendered frame.
	 * @param frameActions The frame-level action snapshot to copy.
	 */
	void LatchFrame(const virasa::input::ActionState& frameActions);

	/**
	 * @brief Publishes the current per-step action snapshot into the World.
	 * @param world World whose ActionState resource binding is updated.
	 * @param firstStep True for the first fixed step of the current frame.
	 */
	void Publish(virasa::ecs::World& world, bool firstStep);

	/**
	 * @brief Returns the ActionState resource currently bound in a World.
	 * @param world World whose input resource binding is queried.
	 * @return The bound ActionState, or nullptr when none is bound.
	 */
	[[nodiscard]] static const virasa::input::ActionState* GetActions(
		const virasa::ecs::World& world) noexcept;

	private:
	virasa::input::ActionState _frame;
	virasa::input::ActionState _step;
};

} // namespace virasa::input

#endif // VIRASA_INPUT_STEPBRIDGE_H
