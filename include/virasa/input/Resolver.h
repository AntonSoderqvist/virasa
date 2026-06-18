// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#ifndef VIRASA_INPUT_RESOLVER_H
#define VIRASA_INPUT_RESOLVER_H

#include "virasa/input/Actions.h"
#include "virasa/input/Bindings.h"
#include "virasa/window/Events.h"
#include "virasa/window/InputState.h"

namespace virasa::window
{

using InputState = virasa::InputState;

} // namespace virasa::window

namespace virasa::input
{

/**
 * @brief Maps sampled window input and authored bindings to action state.
 *
 * Resolver is stateless. Each Resolve call returns a freshly computed
 * ActionState snapshot without mutating the input state or binding table.
 */
class Resolver final
{
	public:
	/**
	 * @brief Resolves input controls into semantic action state.
	 * @param input Sampled keyboard and mouse state to read.
	 * @param bindings Authored input bindings to apply.
	 * @return A freshly computed action state snapshot.
	 */
	[[nodiscard]] virasa::input::ActionState Resolve(
		const virasa::window::InputState& input,
		const virasa::input::Bindings& bindings) const;
};

} // namespace virasa::input

#endif // VIRASA_INPUT_RESOLVER_H
