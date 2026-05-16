#include "virasa/window/inputstate.h"

#include <cstring>

namespace virasa
{

void InputState::Update(std::span<const Event> events)
{
	// Step 1: snapshot current key state as previous-frame state and clear per-frame deltas
	std::memcpy(_previousKeys, _currentKeys, sizeof(_currentKeys));
	_mouseDeltaX = 0;
	_mouseDeltaY = 0;
	_scrollDeltaX = 0.0f;
	_scrollDeltaY = 0.0f;

	// Step 2: apply each event in order
	for (const Event& event : events)
	{
		switch (event.type)
		{
			case EventType::KeyDown:
			{
				const KeyCode key = event.keyboard.key;
				if (key != KeyCode::Unknown)
				{
					const int idx = static_cast<int>(key);
					if (idx >= 0 && idx < kKeyCount)
					{
						_currentKeys[idx] = true;
					}
				}
				break;
			}
			case EventType::KeyUp:
			{
				const KeyCode key = event.keyboard.key;
				if (key != KeyCode::Unknown)
				{
					const int idx = static_cast<int>(key);
					if (idx >= 0 && idx < kKeyCount)
					{
						_currentKeys[idx] = false;
					}
				}
				break;
			}
			case EventType::MouseButtonDown:
			{
				const int idx = static_cast<int>(event.mouseButton.button);
				if (idx >= 0 && idx < kMouseButtonCount)
				{
					_currentMouseButtons[idx] = true;
				}
				break;
			}
			case EventType::MouseButtonUp:
			{
				const int idx = static_cast<int>(event.mouseButton.button);
				if (idx >= 0 && idx < kMouseButtonCount)
				{
					_currentMouseButtons[idx] = false;
				}
				break;
			}
			case EventType::MouseMoved:
			{
				_mouseX = event.mouseMove.x;
				_mouseY = event.mouseMove.y;
				_mouseDeltaX += event.mouseMove.deltaX;
				_mouseDeltaY += event.mouseMove.deltaY;
				break;
			}
			case EventType::MouseWheel:
			{
				_scrollDeltaX += event.mouseWheel.scrollX;
				_scrollDeltaY += event.mouseWheel.scrollY;
				break;
			}
			default:
				// Ignore event types not consumed by InputState
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
	const int idx = static_cast<int>(key);
	if (idx < 0 || idx >= kKeyCount)
	{
		return false;
	}
	return _currentKeys[idx];
}

bool InputState::WasKeyPressed(KeyCode key) const
{
	if (key == KeyCode::Unknown)
	{
		return false;
	}
	const int idx = static_cast<int>(key);
	if (idx < 0 || idx >= kKeyCount)
	{
		return false;
	}
	return _currentKeys[idx] && !_previousKeys[idx];
}

bool InputState::WasKeyReleased(KeyCode key) const
{
	if (key == KeyCode::Unknown)
	{
		return false;
	}
	const int idx = static_cast<int>(key);
	if (idx < 0 || idx >= kKeyCount)
	{
		return false;
	}
	return !_currentKeys[idx] && _previousKeys[idx];
}

bool InputState::IsMouseButtonDown(MouseButton button) const
{
	const int idx = static_cast<int>(button);
	if (idx < 0 || idx >= kMouseButtonCount)
	{
		return false;
	}
	return _currentMouseButtons[idx];
}

std::pair<int32_t, int32_t> InputState::GetMousePosition() const
{
	return { _mouseX, _mouseY };
}

std::pair<int32_t, int32_t> InputState::GetMouseDelta() const
{
	return { _mouseDeltaX, _mouseDeltaY };
}

std::pair<float, float> InputState::GetScrollDelta() const
{
	return { _scrollDeltaX, _scrollDeltaY };
}

void InputState::Reset()
{
	std::memset(_currentKeys, 0, sizeof(_currentKeys));
	std::memset(_previousKeys, 0, sizeof(_previousKeys));
	std::memset(_currentMouseButtons, 0, sizeof(_currentMouseButtons));
	_mouseX = 0;
	_mouseY = 0;
	_mouseDeltaX = 0;
	_mouseDeltaY = 0;
	_scrollDeltaX = 0.0f;
	_scrollDeltaY = 0.0f;
}

} // namespace virasa
