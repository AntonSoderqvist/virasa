// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#include "virasa/input/Resolver.h"

#include <algorithm>
#include <cstddef>

namespace virasa::input
{

virasa::input::ActionState Resolver::Resolve(
	const virasa::window::InputState& input,
	const virasa::input::Bindings& bindings) const
{
	virasa::input::ActionState state;
	float axisValues[virasa::input::ActionState::kMaxActions] = {};

	for (const virasa::input::DigitalBinding& binding : bindings.DigitalBindings())
	{
		const bool held = binding.useMouse
			? input.IsMouseButtonDown(binding.button)
			: input.IsKeyDown(binding.key);
		const bool pressed = binding.useMouse ? false : input.WasKeyPressed(binding.key);
		const bool released = binding.useMouse ? false : input.WasKeyReleased(binding.key);

		state.SetButton(
			binding.action,
			state.Held(binding.action) || held,
			state.Pressed(binding.action) || pressed,
			state.Released(binding.action) || released);
	}

	for (const virasa::input::AxisBinding& binding : bindings.AxisBindings())
	{
		if (static_cast<std::size_t>(binding.action) >= virasa::input::ActionState::kMaxActions)
		{
			continue;
		}

		const float positive = input.IsKeyDown(binding.positive) ? 1.0f : 0.0f;
		const float negative = input.IsKeyDown(binding.negative) ? 1.0f : 0.0f;
		axisValues[static_cast<std::size_t>(binding.action)] +=
			(positive - negative) * binding.scale;
	}

	for (std::size_t index = 0; index < virasa::input::ActionState::kMaxActions; ++index)
	{
		state.SetAxis(
			static_cast<virasa::input::ActionId>(index),
			std::clamp(axisValues[index], -1.0f, 1.0f));
	}

	return state;
}

} // namespace virasa::input
