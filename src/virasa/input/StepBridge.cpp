// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#include "virasa/input/StepBridge.h"

#include "virasa/ecs/World.h"

#include <cstddef>
#include <typeindex>
#include <typeinfo>

namespace virasa::input
{

void StepBridge::LatchFrame(const virasa::input::ActionState& frameActions)
{
	_frame = frameActions;
}

void StepBridge::Publish(virasa::ecs::World& world, bool firstStep)
{
	for (std::size_t index = 0; index < virasa::input::ActionState::kMaxActions; ++index)
	{
		const auto action = static_cast<virasa::input::ActionId>(index);

		_step.SetAxis(action, _frame.Axis(action));
		_step.SetButton(
			action,
			_frame.Held(action),
			firstStep && _frame.Pressed(action),
			firstStep && _frame.Released(action));
	}

	world.SetResource(std::type_index(typeid(virasa::input::ActionState)), &_step);
}

const virasa::input::ActionState* StepBridge::GetActions(
	const virasa::ecs::World& world) noexcept
{
	return static_cast<const virasa::input::ActionState*>(
		world.GetResource(std::type_index(typeid(virasa::input::ActionState))));
}

} // namespace virasa::input
