// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#ifndef VIRASA_INPUT_ACTIONS_H
#define VIRASA_INPUT_ACTIONS_H

#include <cstddef>
#include <cstdint>

namespace virasa::input
{

/**
 * @brief Dense handle identifying a semantic input action.
 *
 * ActionId values in the range [0, ActionState::kMaxActions) select action
 * slots. Values outside that range, including ActionState::kInvalidActionId,
 * do not select a valid action slot.
 */
using ActionId = std::uint16_t;

/**
 * @brief Per-step snapshot of semantic input action state.
 *
 * ActionState stores an axis value plus held, pressed, and released button
 * state for each supported action. Out-of-range action queries return neutral
 * values, and out-of-range setters are ignored.
 */
class ActionState final
{
	public:
	/// @brief Number of action slots stored in each ActionState.
	static constexpr std::size_t kMaxActions = 64u;

	/// @brief Sentinel value representing no valid action.
	static constexpr virasa::input::ActionId kInvalidActionId =
		static_cast<virasa::input::ActionId>(0xFFFFu);

	/**
	 * @brief Returns the stored axis value for an action.
	 * @param action The action slot to query.
	 * @return The stored axis value, or 0.0f when action is out of range.
	 */
	[[nodiscard]] float Axis(virasa::input::ActionId action) const noexcept
	{
		if (static_cast<std::size_t>(action) >= kMaxActions)
		{
			return 0.0f;
		}

		return _axis[static_cast<std::size_t>(action)];
	}

	/**
	 * @brief Returns whether an action is currently held.
	 * @param action The action slot to query.
	 * @return True when the in-range action is held; false otherwise.
	 */
	[[nodiscard]] bool Held(virasa::input::ActionId action) const noexcept
	{
		if (static_cast<std::size_t>(action) >= kMaxActions)
		{
			return false;
		}

		return _held[static_cast<std::size_t>(action)];
	}

	/**
	 * @brief Returns whether an action was pressed this step.
	 * @param action The action slot to query.
	 * @return True when the in-range action was pressed; false otherwise.
	 */
	[[nodiscard]] bool Pressed(virasa::input::ActionId action) const noexcept
	{
		if (static_cast<std::size_t>(action) >= kMaxActions)
		{
			return false;
		}

		return _pressed[static_cast<std::size_t>(action)];
	}

	/**
	 * @brief Returns whether an action was released this step.
	 * @param action The action slot to query.
	 * @return True when the in-range action was released; false otherwise.
	 */
	[[nodiscard]] bool Released(virasa::input::ActionId action) const noexcept
	{
		if (static_cast<std::size_t>(action) >= kMaxActions)
		{
			return false;
		}

		return _released[static_cast<std::size_t>(action)];
	}

	/**
	 * @brief Stores an axis value for an action.
	 * @param action The action slot to update.
	 * @param value The axis value to store.
	 */
	void SetAxis(virasa::input::ActionId action, float value)
	{
		if (static_cast<std::size_t>(action) >= kMaxActions)
		{
			return;
		}

		_axis[static_cast<std::size_t>(action)] = value;
	}

	/**
	 * @brief Stores button state for an action.
	 * @param action The action slot to update.
	 * @param held Whether the action is currently held.
	 * @param pressed Whether the action was pressed this step.
	 * @param released Whether the action was released this step.
	 */
	void SetButton(
		virasa::input::ActionId action,
		bool held,
		bool pressed,
		bool released)
	{
		if (static_cast<std::size_t>(action) >= kMaxActions)
		{
			return;
		}

		const std::size_t index = static_cast<std::size_t>(action);
		_held[index] = held;
		_pressed[index] = pressed;
		_released[index] = released;
	}

	/**
	 * @brief Resets all action state to default neutral values.
	 */
	void Clear()
	{
		for (std::size_t index = 0; index < kMaxActions; ++index)
		{
			_axis[index] = 0.0f;
			_held[index] = false;
			_pressed[index] = false;
			_released[index] = false;
		}
	}

	private:
	float _axis[kMaxActions] = {};
	bool _held[kMaxActions] = {};
	bool _pressed[kMaxActions] = {};
	bool _released[kMaxActions] = {};
};

} // namespace virasa::input

#endif // VIRASA_INPUT_ACTIONS_H
