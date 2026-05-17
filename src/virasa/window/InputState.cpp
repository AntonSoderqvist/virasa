// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#include "virasa/window/InputState.h"

#include <cstddef>

namespace virasa
{

void InputState::Update(std::span<const Event> events)
{
	// Step 1: snapshot current state as previous, clear per-frame deltas.
	for (std::size_t i = 0; i < kKeyCount; ++i)
	{
		_keyPrevious[i] = _keyCurrent[i];
	}

	_mouseDeltaX = 0;
	_mouseDeltaY = 0;
	_scrollX = 0.0f;
	_scrollY = 0.0f;

	// Step 2: consume events in order.
	for (const Event& ev : events)
	{
		switch (ev.type)
		{
			case EventType::KeyDown:
			{
				const KeyCode key = ev.keyboard.key;
				if (key != KeyCode::Unknown)
				{
					const std::size_t idx = static_cast<std::size_t>(key);
					if (idx < kKeyCount)
					{
						_keyCurrent[idx] = true;
					}
				}
				break;
			}
			case EventType::KeyUp:
			{
				const KeyCode key = ev.keyboard.key;
				if (key != KeyCode::Unknown)
				{
					const std::size_t idx = static_cast<std::size_t>(key);
					if (idx < kKeyCount)
					{
						_keyCurrent[idx] = false;
					}
				}
				break;
			}
			case EventType::MouseButtonDown:
			{
				const std::size_t idx = static_cast<std::size_t>(ev.mouseButton.button);
				if (idx < kMouseButtonCount)
				{
					_mouseCurrent[idx] = true;
				}
				break;
			}
			case EventType::MouseButtonUp:
			{
				const std::size_t idx = static_cast<std::size_t>(ev.mouseButton.button);
				if (idx < kMouseButtonCount)
				{
					_mouseCurrent[idx] = false;
				}
				break;
			}
			case EventType::MouseMoved:
			{
				_mouseX = ev.mouseMove.x;
				_mouseY = ev.mouseMove.y;
				_mouseDeltaX += ev.mouseMove.deltaX;
				_mouseDeltaY += ev.mouseMove.deltaY;
				break;
			}
			case EventType::MouseWheel:
			{
				_scrollX += ev.mouseWheel.scrollX;
				_scrollY += ev.mouseWheel.scrollY;
				break;
			}
			default:
				break;
		}
	}
}

bool InputState::IsKeyDown(KeyCode key) const
{
	if (key == KeyCode::Unknown)
	{
		return false;
	}
	const std::size_t idx = static_cast<std::size_t>(key);
	if (idx >= kKeyCount)
	{
		return false;
	}
	return _keyCurrent[idx];
}

bool InputState::WasKeyPressed(KeyCode key) const
{
	if (key == KeyCode::Unknown)
	{
		return false;
	}
	const std::size_t idx = static_cast<std::size_t>(key);
	if (idx >= kKeyCount)
	{
		return false;
	}
	return _keyCurrent[idx] && !_keyPrevious[idx];
}

bool InputState::WasKeyReleased(KeyCode key) const
{
	if (key == KeyCode::Unknown)
	{
		return false;
	}
	const std::size_t idx = static_cast<std::size_t>(key);
	if (idx >= kKeyCount)
	{
		return false;
	}
	return !_keyCurrent[idx] && _keyPrevious[idx];
}

bool InputState::IsMouseButtonDown(MouseButton button) const
{
	const std::size_t idx = static_cast<std::size_t>(button);
	if (idx >= kMouseButtonCount)
	{
		return false;
	}
	return _mouseCurrent[idx];
}

std::pair<int32_t, int32_t> InputState::GetMousePosition() const
{
	return {_mouseX, _mouseY};
}

std::pair<int32_t, int32_t> InputState::GetMouseDelta() const
{
	return {_mouseDeltaX, _mouseDeltaY};
}

std::pair<float, float> InputState::GetScrollDelta() const
{
	return {_scrollX, _scrollY};
}

void InputState::Reset()
{
	for (std::size_t i = 0; i < kKeyCount; ++i)
	{
		_keyCurrent[i] = false;
		_keyPrevious[i] = false;
	}
	for (std::size_t i = 0; i < kMouseButtonCount; ++i)
	{
		_mouseCurrent[i] = false;
	}
	_mouseX = 0;
	_mouseY = 0;
	_mouseDeltaX = 0;
	_mouseDeltaY = 0;
	_scrollX = 0.0f;
	_scrollY = 0.0f;
}

} // namespace virasa
