// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#ifndef VIRASA_WINDOW_EVENTS_H
#define VIRASA_WINDOW_EVENTS_H

#include <cstdint>

namespace virasa
{

/**
 * @brief Identifies the type of a window or input event.
 *
 * None is the absence of an event (value 0). Count is a sentinel equal to
 * the total number of preceding event types; it is not a valid event type.
 */
enum class EventType : uint8_t
{
	None = 0,
	Quit,
	WindowResized,
	WindowMinimized,
	WindowRestored,
	WindowFocusGained,
	WindowFocusLost,
	KeyDown,
	KeyUp,
	MouseButtonDown,
	MouseButtonUp,
	MouseMoved,
	MouseWheel,
	TextInput,
	Count
};

/**
 * @brief Physical key identifiers.
 *
 * Unknown is the first value (0) and represents an unmapped or unrecognized key.
 * Count is the last value and is a sentinel; it is not a valid key code.
 */
enum class KeyCode : uint16_t
{
	Unknown = 0,
	A,
	B,
	C,
	D,
	E,
	F,
	G,
	H,
	I,
	J,
	K,
	L,
	M,
	N,
	O,
	P,
	Q,
	R,
	S,
	T,
	U,
	V,
	W,
	X,
	Y,
	Z,
	Num0,
	Num1,
	Num2,
	Num3,
	Num4,
	Num5,
	Num6,
	Num7,
	Num8,
	Num9,
	Escape,
	Enter,
	Tab,
	Space,
	Backspace,
	Left,
	Right,
	Up,
	Down,
	LShift,
	RShift,
	LCtrl,
	RCtrl,
	LAlt,
	RAlt,
	F1,
	F2,
	F3,
	F4,
	F5,
	F6,
	F7,
	F8,
	F9,
	F10,
	F11,
	F12,
	Count
};

/**
 * @brief Mouse button identifiers.
 *
 * Left is the first value (0). X1 and X2 are the extra buttons
 * (conventionally "back" and "forward"). There is no Count sentinel.
 */
enum class MouseButton : uint8_t
{
	Left = 0,
	Middle,
	Right,
	X1,
	X2
};

/**
 * @brief Fixed-size UTF-8 text input payload.
 *
 * utf8 is a 32-byte buffer always NUL-terminated at index length.
 * The maximum payload is 31 bytes (one byte reserved for the NUL terminator).
 */
struct TextInputData
{
public:
	/// UTF-8 encoded text, NUL-terminated at index length.
	char utf8[32];
	/// Number of payload bytes in utf8, excluding the NUL terminator.
	uint8_t length;

	/// @brief Default-constructs a TextInputData with an empty buffer.
	TextInputData() noexcept : utf8{}, length(0) {}
};

/**
 * @brief A single window or input event.
 *
 * Every Event carries a public EventType and a uint64_t timestamp (nanoseconds).
 * Additional payload data is present and valid only when type matches the
 * payload's corresponding event type. Reading a payload for a non-matching
 * type is undefined behaviour.
 *
 * A default-constructed Event has type == EventType::None and timestamp == 0.
 */
struct Event
{
public:
	/// The kind of event.
	EventType type;
	/// Event time in nanoseconds as provided by the platform layer.
	uint64_t timestamp;

	/**
	 * @brief Keyboard key event payload.
	 *
	 * Valid when type is KeyDown or KeyUp.
	 */
	struct KeyboardPayload
	{
	public:
		/// The key that was pressed or released.
		KeyCode key;
		/// True if this is a key-repeat event.
		bool repeat;
	};

	/**
	 * @brief Mouse button event payload.
	 *
	 * Valid when type is MouseButtonDown or MouseButtonUp.
	 */
	struct MouseButtonPayload
	{
	public:
		/// The button that was pressed or released.
		MouseButton button;
		/// Cursor X position in pixels at the time of the event.
		int32_t x;
		/// Cursor Y position in pixels at the time of the event.
		int32_t y;
	};

	/**
	 * @brief Mouse movement event payload.
	 *
	 * Valid when type is MouseMoved.
	 */
	struct MouseMovePayload
	{
	public:
		/// Absolute cursor X position in pixels.
		int32_t x;
		/// Absolute cursor Y position in pixels.
		int32_t y;
		/// Relative X movement since the last event.
		int32_t deltaX;
		/// Relative Y movement since the last event.
		int32_t deltaY;
	};

	/**
	 * @brief Mouse wheel event payload.
	 *
	 * Valid when type is MouseWheel.
	 */
	struct MouseWheelPayload
	{
	public:
		/// Horizontal scroll amount.
		float scrollX;
		/// Vertical scroll amount.
		float scrollY;
	};

	/**
	 * @brief Window resize event payload.
	 *
	 * Valid when type is WindowResized.
	 */
	struct ResizePayload
	{
	public:
		/// New window width in pixels.
		uint32_t width;
		/// New window height in pixels.
		uint32_t height;
	};

	/// Per-event-type payload storage.
	union
	{
		/// Valid when type is KeyDown or KeyUp.
		KeyboardPayload keyboard;

		/// Valid when type is MouseButtonDown or MouseButtonUp.
		MouseButtonPayload mouseButton;

		/// Valid when type is MouseMoved.
		MouseMovePayload mouseMove;

		/// Valid when type is MouseWheel.
		MouseWheelPayload mouseWheel;

		/// Valid when type is WindowResized.
		ResizePayload resize;

		/// Valid when type is TextInput.
		TextInputData textInput;
	};

	/// @brief Default-constructs an Event with type None and timestamp 0.
	Event() noexcept
	    : type(EventType::None), timestamp(0),
		mouseMove{0, 0, 0, 0} // zero-init the largest union member
	{
	}
};

} // namespace virasa

#endif // VIRASA_WINDOW_EVENTS_H
