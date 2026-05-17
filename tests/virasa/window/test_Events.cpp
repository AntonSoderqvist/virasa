#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>

#include "virasa/window/Events.h"

using namespace virasa;

// ---------------------------------------------------------------------------
// EventType enum
// ---------------------------------------------------------------------------

TEST(Events, test_event_type_enum_values_in_declared_order)
{
	// Underlying type must be uint8_t.
	static_assert(
		sizeof(EventType) == sizeof(uint8_t), "EventType must have uint8_t underlying type");

	// None is explicitly 0.
	EXPECT_EQ(static_cast<uint8_t>(EventType::None), uint8_t{0});

	// Values appear in declared order (each one greater than the previous).
	EXPECT_LT(static_cast<uint8_t>(EventType::None), static_cast<uint8_t>(EventType::Quit));
	EXPECT_LT(
		static_cast<uint8_t>(EventType::Quit), static_cast<uint8_t>(EventType::WindowResized));
	EXPECT_LT(static_cast<uint8_t>(EventType::WindowResized),
		static_cast<uint8_t>(EventType::WindowMinimized));
	EXPECT_LT(static_cast<uint8_t>(EventType::WindowMinimized),
		static_cast<uint8_t>(EventType::WindowRestored));
	EXPECT_LT(static_cast<uint8_t>(EventType::WindowRestored),
		static_cast<uint8_t>(EventType::WindowFocusGained));
	EXPECT_LT(static_cast<uint8_t>(EventType::WindowFocusGained),
		static_cast<uint8_t>(EventType::WindowFocusLost));
	EXPECT_LT(static_cast<uint8_t>(EventType::WindowFocusLost),
		static_cast<uint8_t>(EventType::KeyDown));
	EXPECT_LT(static_cast<uint8_t>(EventType::KeyDown), static_cast<uint8_t>(EventType::KeyUp));
	EXPECT_LT(static_cast<uint8_t>(EventType::KeyUp),
		static_cast<uint8_t>(EventType::MouseButtonDown));
	EXPECT_LT(static_cast<uint8_t>(EventType::MouseButtonDown),
		static_cast<uint8_t>(EventType::MouseButtonUp));
	EXPECT_LT(static_cast<uint8_t>(EventType::MouseButtonUp),
		static_cast<uint8_t>(EventType::MouseMoved));
	EXPECT_LT(static_cast<uint8_t>(EventType::MouseMoved),
		static_cast<uint8_t>(EventType::MouseWheel));
	EXPECT_LT(static_cast<uint8_t>(EventType::MouseWheel),
		static_cast<uint8_t>(EventType::TextInput));
	EXPECT_LT(static_cast<uint8_t>(EventType::TextInput), static_cast<uint8_t>(EventType::Count));

	// Count equals the number of preceding values (14 values before Count: None..TextInput).
	EXPECT_EQ(static_cast<uint8_t>(EventType::Count), uint8_t{14});
}

// ---------------------------------------------------------------------------
// KeyCode enum
// ---------------------------------------------------------------------------

TEST(Events, test_key_code_enum_values_in_declared_order)
{
	// Underlying type must be uint16_t.
	static_assert(
		sizeof(KeyCode) == sizeof(uint16_t), "KeyCode must have uint16_t underlying type");

	// Unknown is explicitly 0.
	EXPECT_EQ(static_cast<uint16_t>(KeyCode::Unknown), uint16_t{0});

	// Spot-check ordering through all declared groups.
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Unknown), static_cast<uint16_t>(KeyCode::A));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::A), static_cast<uint16_t>(KeyCode::B));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Y), static_cast<uint16_t>(KeyCode::Z));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Z), static_cast<uint16_t>(KeyCode::Num0));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Num0), static_cast<uint16_t>(KeyCode::Num1));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Num8), static_cast<uint16_t>(KeyCode::Num9));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Num9), static_cast<uint16_t>(KeyCode::Escape));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Escape), static_cast<uint16_t>(KeyCode::Enter));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Enter), static_cast<uint16_t>(KeyCode::Tab));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Tab), static_cast<uint16_t>(KeyCode::Space));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Space), static_cast<uint16_t>(KeyCode::Backspace));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Backspace), static_cast<uint16_t>(KeyCode::Left));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Left), static_cast<uint16_t>(KeyCode::Right));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Right), static_cast<uint16_t>(KeyCode::Up));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Up), static_cast<uint16_t>(KeyCode::Down));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::Down), static_cast<uint16_t>(KeyCode::LShift));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::LShift), static_cast<uint16_t>(KeyCode::RShift));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::RShift), static_cast<uint16_t>(KeyCode::LCtrl));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::LCtrl), static_cast<uint16_t>(KeyCode::RCtrl));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::RCtrl), static_cast<uint16_t>(KeyCode::LAlt));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::LAlt), static_cast<uint16_t>(KeyCode::RAlt));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::RAlt), static_cast<uint16_t>(KeyCode::F1));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::F1), static_cast<uint16_t>(KeyCode::F2));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::F11), static_cast<uint16_t>(KeyCode::F12));
	EXPECT_LT(static_cast<uint16_t>(KeyCode::F12), static_cast<uint16_t>(KeyCode::Count));

	// Count is a sentinel greater than all valid key codes.
	EXPECT_GT(static_cast<uint16_t>(KeyCode::Count), static_cast<uint16_t>(KeyCode::F12));
}

// ---------------------------------------------------------------------------
// MouseButton enum
// ---------------------------------------------------------------------------

TEST(Events, test_mouse_button_enum_values_in_declared_order)
{
	// Underlying type must be uint8_t.
	static_assert(sizeof(MouseButton) == sizeof(uint8_t),
		"MouseButton must have uint8_t underlying type");

	// Left is explicitly 0.
	EXPECT_EQ(static_cast<uint8_t>(MouseButton::Left), uint8_t{0});

	// Values appear in declared order.
	EXPECT_LT(static_cast<uint8_t>(MouseButton::Left), static_cast<uint8_t>(MouseButton::Middle));
	EXPECT_LT(
		static_cast<uint8_t>(MouseButton::Middle), static_cast<uint8_t>(MouseButton::Right));
	EXPECT_LT(static_cast<uint8_t>(MouseButton::Right), static_cast<uint8_t>(MouseButton::X1));
	EXPECT_LT(static_cast<uint8_t>(MouseButton::X1), static_cast<uint8_t>(MouseButton::X2));
}

// ---------------------------------------------------------------------------
// TextInputData
// ---------------------------------------------------------------------------

TEST(Events, test_text_input_data_buffer_is_fixed_32_bytes)
{
	// utf8 member is exactly 32 bytes.
	static_assert(sizeof(TextInputData::utf8) == 32, "utf8 buffer must be 32 bytes");

	// length is uint8_t.
	static_assert(sizeof(TextInputData::length) == sizeof(uint8_t), "length must be uint8_t");

	// Default-constructed: length == 0 and utf8 is zero-initialized.
	TextInputData tid;
	EXPECT_EQ(tid.length, uint8_t{0});
	for (int i = 0; i < 32; ++i)
	{
		EXPECT_EQ(tid.utf8[i], '\0')
			<< "utf8[" << i << "] should be zero after default construction";
	}

	// Verify copyability.
	TextInputData copy = tid;
	EXPECT_EQ(copy.length, tid.length);

	// Verify movability.
	TextInputData moved = std::move(copy);
	EXPECT_EQ(moved.length, tid.length);
}

// ---------------------------------------------------------------------------
// Event — type and timestamp members
// ---------------------------------------------------------------------------

TEST(Events, test_event_carries_type_and_timestamp)
{
	Event ev;
	ev.type = EventType::KeyDown;
	ev.timestamp = UINT64_C(123456789012345);

	EXPECT_EQ(ev.type, EventType::KeyDown);
	EXPECT_EQ(ev.timestamp, UINT64_C(123456789012345));

	// Verify copyability.
	Event copy = ev;
	EXPECT_EQ(copy.type, EventType::KeyDown);
	EXPECT_EQ(copy.timestamp, UINT64_C(123456789012345));

	// Verify movability.
	Event moved = std::move(copy);
	EXPECT_EQ(moved.type, EventType::KeyDown);
	EXPECT_EQ(moved.timestamp, UINT64_C(123456789012345));
}

// ---------------------------------------------------------------------------
// Event — default construction
// ---------------------------------------------------------------------------

TEST(Events, test_event_default_constructs_to_none_with_zero_timestamp)
{
	Event ev;
	EXPECT_EQ(ev.type, EventType::None);
	EXPECT_EQ(ev.timestamp, uint64_t{0});
}

// ---------------------------------------------------------------------------
// Event — payload access discipline
// ---------------------------------------------------------------------------

TEST(Events, test_event_payload_is_valid_only_for_matching_type)
{
	// KeyDown — keyboard payload.
	{
		Event ev;
		ev.type = EventType::KeyDown;
		ev.timestamp = 1u;
		ev.keyboard.key = KeyCode::A;
		ev.keyboard.repeat = false;
		EXPECT_EQ(ev.keyboard.key, KeyCode::A);
		EXPECT_EQ(ev.keyboard.repeat, false);
	}

	// KeyUp — keyboard payload with repeat.
	{
		Event ev;
		ev.type = EventType::KeyUp;
		ev.timestamp = 2u;
		ev.keyboard.key = KeyCode::Escape;
		ev.keyboard.repeat = true;
		EXPECT_EQ(ev.keyboard.key, KeyCode::Escape);
		EXPECT_EQ(ev.keyboard.repeat, true);
	}

	// MouseButtonDown — mouseButton payload.
	{
		Event ev;
		ev.type = EventType::MouseButtonDown;
		ev.timestamp = 3u;
		ev.mouseButton.button = MouseButton::Right;
		ev.mouseButton.x = 100;
		ev.mouseButton.y = 200;
		EXPECT_EQ(ev.mouseButton.button, MouseButton::Right);
		EXPECT_EQ(ev.mouseButton.x, int32_t{100});
		EXPECT_EQ(ev.mouseButton.y, int32_t{200});
	}

	// MouseButtonUp — mouseButton payload.
	{
		Event ev;
		ev.type = EventType::MouseButtonUp;
		ev.timestamp = 4u;
		ev.mouseButton.button = MouseButton::Left;
		ev.mouseButton.x = 50;
		ev.mouseButton.y = 75;
		EXPECT_EQ(ev.mouseButton.button, MouseButton::Left);
		EXPECT_EQ(ev.mouseButton.x, int32_t{50});
		EXPECT_EQ(ev.mouseButton.y, int32_t{75});
	}

	// MouseMoved — mouseMove payload.
	{
		Event ev;
		ev.type = EventType::MouseMoved;
		ev.timestamp = 5u;
		ev.mouseMove.x = 320;
		ev.mouseMove.y = 240;
		ev.mouseMove.deltaX = -5;
		ev.mouseMove.deltaY = 3;
		EXPECT_EQ(ev.mouseMove.x, int32_t{320});
		EXPECT_EQ(ev.mouseMove.y, int32_t{240});
		EXPECT_EQ(ev.mouseMove.deltaX, int32_t{-5});
		EXPECT_EQ(ev.mouseMove.deltaY, int32_t{3});
	}

	// MouseWheel — mouseWheel payload.
	{
		Event ev;
		ev.type = EventType::MouseWheel;
		ev.timestamp = 6u;
		ev.mouseWheel.scrollX = 0.0f;
		ev.mouseWheel.scrollY = -1.5f;
		EXPECT_FLOAT_EQ(ev.mouseWheel.scrollX, 0.0f);
		EXPECT_FLOAT_EQ(ev.mouseWheel.scrollY, -1.5f);
	}

	// WindowResized — resize payload.
	{
		Event ev;
		ev.type = EventType::WindowResized;
		ev.timestamp = 7u;
		ev.resize.width = 1920u;
		ev.resize.height = 1080u;
		EXPECT_EQ(ev.resize.width, uint32_t{1920});
		EXPECT_EQ(ev.resize.height, uint32_t{1080});
	}

	// TextInput — textInput payload.
	{
		Event ev;
		ev.type = EventType::TextInput;
		ev.timestamp = 8u;
		const char kText[] = "hello";
		const uint8_t kLen = static_cast<uint8_t>(sizeof(kText) - 1u);
		std::memcpy(ev.textInput.utf8, kText, sizeof(kText));
		ev.textInput.length = kLen;
		EXPECT_EQ(ev.textInput.length, kLen);
		EXPECT_STREQ(ev.textInput.utf8, "hello");
	}
}
