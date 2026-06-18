// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#include "virasa/input/Bindings.h"

namespace virasa::input
{

void Bindings::BindDigital(virasa::input::ActionId action, virasa::window::KeyCode key)
{
	_digital.push_back(virasa::input::DigitalBinding{
		.action = action,
		.key = key,
		.button = virasa::window::MouseButton::Left,
		.useMouse = false,
	});
}

void Bindings::BindDigitalMouse(
	virasa::input::ActionId action,
	virasa::window::MouseButton button)
{
	_digital.push_back(virasa::input::DigitalBinding{
		.action = action,
		.key = virasa::window::KeyCode::Unknown,
		.button = button,
		.useMouse = true,
	});
}

void Bindings::BindAxis(
	virasa::input::ActionId action,
	virasa::window::KeyCode positive,
	virasa::window::KeyCode negative,
	float scale)
{
	_axis.push_back(virasa::input::AxisBinding{
		.action = action,
		.positive = positive,
		.negative = negative,
		.scale = scale,
	});
}

std::span<const virasa::input::DigitalBinding> Bindings::DigitalBindings() const noexcept
{
	return _digital;
}

std::span<const virasa::input::AxisBinding> Bindings::AxisBindings() const noexcept
{
	return _axis;
}

void Bindings::Clear()
{
	_digital.clear();
	_axis.clear();
}

} // namespace virasa::input
