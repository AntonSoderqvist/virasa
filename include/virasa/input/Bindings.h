// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#ifndef VIRASA_INPUT_BINDINGS_H
#define VIRASA_INPUT_BINDINGS_H

#include "virasa/input/Actions.h"
#include "virasa/window/Events.h"

#include <span>
#include <vector>

namespace virasa::window
{

using KeyCode = virasa::KeyCode;
using MouseButton = virasa::MouseButton;

} // namespace virasa::window

namespace virasa::input
{

/**
 * @brief Maps one digital keyboard key or mouse button to an action.
 *
 * When useMouse is false, key is the active control and button is ignored.
 * When useMouse is true, button is the active control and key is ignored.
 */
struct DigitalBinding
{
	public:
	/// @brief Action receiving the digital control state.
	virasa::input::ActionId action = virasa::input::ActionState::kInvalidActionId;

	/// @brief Keyboard key used when useMouse is false.
	virasa::window::KeyCode key = virasa::window::KeyCode::Unknown;

	/// @brief Mouse button used when useMouse is true.
	virasa::window::MouseButton button = virasa::window::MouseButton::Left;

	/// @brief Selects whether button or key drives this binding.
	bool useMouse = false;
};

/**
 * @brief Maps a pair of keyboard keys to a signed action axis.
 *
 * The positive key contributes +scale while held and the negative key
 * contributes -scale while held. Holding both keys cancels to zero.
 */
struct AxisBinding
{
	public:
	/// @brief Action receiving the composed axis value.
	virasa::input::ActionId action = virasa::input::ActionState::kInvalidActionId;

	/// @brief Key that pushes the axis in the positive direction.
	virasa::window::KeyCode positive = virasa::window::KeyCode::Unknown;

	/// @brief Key that pushes the axis in the negative direction.
	virasa::window::KeyCode negative = virasa::window::KeyCode::Unknown;

	/// @brief Multiplier applied to the composed axis contribution.
	float scale = 1.0f;
};

/**
 * @brief Mutable table of authored digital and axis input bindings.
 *
 * Bindings owns insertion-ordered digital and axis binding lists. It performs
 * no input sampling and does not validate action ids.
 */
class Bindings final
{
	public:
	/**
	 * @brief Appends a keyboard digital binding.
	 * @param action Action receiving the key state.
	 * @param key Keyboard key to bind.
	 */
	void BindDigital(virasa::input::ActionId action, virasa::window::KeyCode key);

	/**
	 * @brief Appends a mouse-button digital binding.
	 * @param action Action receiving the mouse button state.
	 * @param button Mouse button to bind.
	 */
	void BindDigitalMouse(virasa::input::ActionId action, virasa::window::MouseButton button);

	/**
	 * @brief Appends a keyboard axis binding.
	 * @param action Action receiving the composed axis value.
	 * @param positive Key that contributes in the positive direction.
	 * @param negative Key that contributes in the negative direction.
	 * @param scale Multiplier applied to the composed axis.
	 */
	void BindAxis(
		virasa::input::ActionId action,
		virasa::window::KeyCode positive,
		virasa::window::KeyCode negative,
		float scale);

	/**
	 * @brief Returns all digital bindings in insertion order.
	 * @return A const span over the digital binding list.
	 */
	[[nodiscard]] std::span<const virasa::input::DigitalBinding> DigitalBindings()
		const noexcept;

	/**
	 * @brief Returns all axis bindings in insertion order.
	 * @return A const span over the axis binding list.
	 */
	[[nodiscard]] std::span<const virasa::input::AxisBinding> AxisBindings() const noexcept;

	/**
	 * @brief Removes all digital and axis bindings.
	 */
	void Clear();

	private:
	std::vector<virasa::input::DigitalBinding> _digital;
	std::vector<virasa::input::AxisBinding> _axis;
};

} // namespace virasa::input

#endif // VIRASA_INPUT_BINDINGS_H
