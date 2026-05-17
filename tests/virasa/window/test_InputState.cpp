#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <utility>

#include "virasa/window/Events.h"
#include "virasa/window/InputState.h"

using namespace virasa;

// ---------------------------------------------------------------------------
// Helper factories
// ---------------------------------------------------------------------------
namespace
{

Event MakeKeyDown(KeyCode key, bool repeat = false)
{
	Event e{};
	e.type = EventType::KeyDown;
	e.keyboard.key = key;
	e.keyboard.repeat = repeat;
	return e;
}

Event MakeKeyUp(KeyCode key)
{
	Event e{};
	e.type = EventType::KeyUp;
	e.keyboard.key = key;
	e.keyboard.repeat = false;
	return e;
}

Event MakeMouseButtonDown(MouseButton button, int32_t x = 0, int32_t y = 0)
{
	Event e{};
	e.type = EventType::MouseButtonDown;
	e.mouseButton.button = button;
	e.mouseButton.x = x;
	e.mouseButton.y = y;
	return e;
}

Event MakeMouseButtonUp(MouseButton button, int32_t x = 0, int32_t y = 0)
{
	Event e{};
	e.type = EventType::MouseButtonUp;
	e.mouseButton.button = button;
	e.mouseButton.x = x;
	e.mouseButton.y = y;
	return e;
}

Event MakeMouseMoved(int32_t x, int32_t y, int32_t deltaX, int32_t deltaY)
{
	Event e{};
	e.type = EventType::MouseMoved;
	e.mouseMove.x = x;
	e.mouseMove.y = y;
	e.mouseMove.deltaX = deltaX;
	e.mouseMove.deltaY = deltaY;
	return e;
}

Event MakeMouseWheel(float scrollX, float scrollY)
{
	Event e{};
	e.type = EventType::MouseWheel;
	e.mouseWheel.scrollX = scrollX;
	e.mouseWheel.scrollY = scrollY;
	return e;
}

} // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(InputState, test_input_state_default_constructs_to_all_up_zero_deltas)
{
	InputState state;

	// Every KeyCode (except Count which is a sentinel) must be up.
	const auto keyCount = static_cast<std::size_t>(KeyCode::Count);
	for (std::size_t i = 0; i < keyCount; ++i)
	{
		const auto key = static_cast<KeyCode>(i);
		EXPECT_FALSE(state.IsKeyDown(key)) << "KeyCode index " << i << " should be up";
		EXPECT_FALSE(state.WasKeyPressed(key))
			<< "KeyCode index " << i << " should not be pressed";
		EXPECT_FALSE(state.WasKeyReleased(key))
			<< "KeyCode index " << i << " should not be released";
	}

	// Every MouseButton must be up.
	EXPECT_FALSE(state.IsMouseButtonDown(MouseButton::Left));
	EXPECT_FALSE(state.IsMouseButtonDown(MouseButton::Middle));
	EXPECT_FALSE(state.IsMouseButtonDown(MouseButton::Right));
	EXPECT_FALSE(state.IsMouseButtonDown(MouseButton::X1));
	EXPECT_FALSE(state.IsMouseButtonDown(MouseButton::X2));

	// Position and deltas must be zero.
	const auto [px, py] = state.GetMousePosition();
	EXPECT_EQ(px, 0);
	EXPECT_EQ(py, 0);

	const auto [dx, dy] = state.GetMouseDelta();
	EXPECT_EQ(dx, 0);
	EXPECT_EQ(dy, 0);

	const auto [sx, sy] = state.GetScrollDelta();
	EXPECT_FLOAT_EQ(sx, 0.0f);
	EXPECT_FLOAT_EQ(sy, 0.0f);

	// Verify copyable and movable.
	InputState copy = state;
	(void)copy;
	InputState moved = std::move(copy);
	(void)moved;
}

TEST(InputState, test_update_advances_frame_and_applies_events_in_order)
{
	InputState state;

	// Press A in frame 1.
	{
		const Event events[] = {MakeKeyDown(KeyCode::A)};
		state.Update(events);
	}
	EXPECT_TRUE(state.IsKeyDown(KeyCode::A));
	EXPECT_TRUE(state.WasKeyPressed(KeyCode::A));

	// Empty span in frame 2: A stays down, but WasKeyPressed becomes false.
	{
		state.Update(std::span<const Event>{});
	}
	EXPECT_TRUE(state.IsKeyDown(KeyCode::A));
	EXPECT_FALSE(state.WasKeyPressed(KeyCode::A));
	EXPECT_FALSE(state.WasKeyReleased(KeyCode::A));

	// Empty span also zeroes per-frame deltas.
	{
		// First inject a move event so there is a non-zero delta.
		const Event moveEvents[] = {MakeMouseMoved(10, 20, 5, 7)};
		state.Update(moveEvents);
	}
	{
		state.Update(std::span<const Event>{});
	}
	const auto [dx, dy] = state.GetMouseDelta();
	EXPECT_EQ(dx, 0);
	EXPECT_EQ(dy, 0);

	const auto [sx, sy] = state.GetScrollDelta();
	EXPECT_FLOAT_EQ(sx, 0.0f);
	EXPECT_FLOAT_EQ(sy, 0.0f);

	// Events in a span are applied in order; last event for a key wins.
	InputState state2;
	const Event orderedEvents[] = {
		MakeKeyDown(KeyCode::B),
		MakeKeyUp(KeyCode::B),
		MakeKeyDown(KeyCode::B),
	};
	state2.Update(orderedEvents);
	EXPECT_TRUE(state2.IsKeyDown(KeyCode::B)); // last event was KeyDown
}

TEST(InputState, test_key_events_update_current_key_state)
{
	InputState state;

	// KeyDown sets key to down.
	{
		const Event events[] = {MakeKeyDown(KeyCode::W)};
		state.Update(events);
	}
	EXPECT_TRUE(state.IsKeyDown(KeyCode::W));

	// KeyUp sets key to up.
	{
		const Event events[] = {MakeKeyUp(KeyCode::W)};
		state.Update(events);
	}
	EXPECT_FALSE(state.IsKeyDown(KeyCode::W));

	// Repeat flag does not suppress the KeyDown effect.
	InputState state2;
	{
		const Event events[] = {MakeKeyDown(KeyCode::S, /*repeat=*/true)};
		state2.Update(events);
	}
	EXPECT_TRUE(state2.IsKeyDown(KeyCode::S));

	// KeyCode::Unknown events are ignored.
	InputState state3;
	{
		const Event events[] = {MakeKeyDown(KeyCode::Unknown)};
		state3.Update(events);
	}
	EXPECT_FALSE(state3.IsKeyDown(KeyCode::Unknown));

	// Last event for a key in the span determines final state.
	InputState state4;
	{
		const Event events[] = {
			MakeKeyDown(KeyCode::A),
			MakeKeyUp(KeyCode::A),
		};
		state4.Update(events);
	}
	EXPECT_FALSE(state4.IsKeyDown(KeyCode::A)); // last was KeyUp
}

TEST(InputState, test_was_key_pressed_and_released_are_edge_transitions)
{
	InputState state;

	// Frame 1: press A -> WasKeyPressed true, WasKeyReleased false.
	{
		const Event events[] = {MakeKeyDown(KeyCode::A)};
		state.Update(events);
	}
	EXPECT_TRUE(state.WasKeyPressed(KeyCode::A));
	EXPECT_FALSE(state.WasKeyReleased(KeyCode::A));

	// Frame 2: hold A (no events, stays down) -> neither pressed nor released.
	{
		state.Update(std::span<const Event>{});
	}
	EXPECT_TRUE(state.IsKeyDown(KeyCode::A));
	EXPECT_FALSE(state.WasKeyPressed(KeyCode::A));
	EXPECT_FALSE(state.WasKeyReleased(KeyCode::A));

	// Frame 3: release A -> WasKeyReleased true, WasKeyPressed false.
	{
		const Event events[] = {MakeKeyUp(KeyCode::A)};
		state.Update(events);
	}
	EXPECT_FALSE(state.WasKeyPressed(KeyCode::A));
	EXPECT_TRUE(state.WasKeyReleased(KeyCode::A));

	// Frame 4: key stays up -> neither.
	{
		state.Update(std::span<const Event>{});
	}
	EXPECT_FALSE(state.WasKeyPressed(KeyCode::A));
	EXPECT_FALSE(state.WasKeyReleased(KeyCode::A));

	// Multiple toggles in one frame collapse to a single edge.
	// Start: A is up. Toggle down then up in one frame -> net: up.
	// Previous was up, current is up -> neither pressed nor released.
	InputState state2;
	{
		const Event events[] = {
			MakeKeyDown(KeyCode::A),
			MakeKeyUp(KeyCode::A),
		};
		state2.Update(events);
	}
	EXPECT_FALSE(state2.WasKeyPressed(KeyCode::A));
	EXPECT_FALSE(state2.WasKeyReleased(KeyCode::A));

	// Toggle up then down in one frame -> net: down.
	// Previous was up, current is down -> pressed.
	InputState state3;
	{
		const Event events[] = {
			MakeKeyUp(KeyCode::B), // no-op (already up)
			MakeKeyDown(KeyCode::B),
		};
		state3.Update(events);
	}
	EXPECT_TRUE(state3.WasKeyPressed(KeyCode::B));
	EXPECT_FALSE(state3.WasKeyReleased(KeyCode::B));
}

TEST(InputState, test_mouse_button_events_update_current_button_state)
{
	InputState state;

	// MouseButtonDown sets button to down.
	{
		const Event events[] = {MakeMouseButtonDown(MouseButton::Left, 100, 200)};
		state.Update(events);
	}
	EXPECT_TRUE(state.IsMouseButtonDown(MouseButton::Left));

	// Mouse position should NOT be updated by a button event alone.
	const auto [px, py] = state.GetMousePosition();
	EXPECT_EQ(px, 0);
	EXPECT_EQ(py, 0);

	// MouseButtonUp sets button to up.
	{
		const Event events[] = {MakeMouseButtonUp(MouseButton::Left)};
		state.Update(events);
	}
	EXPECT_FALSE(state.IsMouseButtonDown(MouseButton::Left));

	// Last event for a button in the span determines final state.
	InputState state2;
	{
		const Event events[] = {
			MakeMouseButtonDown(MouseButton::Right),
			MakeMouseButtonUp(MouseButton::Right),
			MakeMouseButtonDown(MouseButton::Right),
		};
		state2.Update(events);
	}
	EXPECT_TRUE(state2.IsMouseButtonDown(MouseButton::Right));

	// All five buttons work independently.
	InputState state3;
	{
		const Event events[] = {
			MakeMouseButtonDown(MouseButton::Left),
			MakeMouseButtonDown(MouseButton::Middle),
			MakeMouseButtonDown(MouseButton::Right),
			MakeMouseButtonDown(MouseButton::X1),
			MakeMouseButtonDown(MouseButton::X2),
		};
		state3.Update(events);
	}
	EXPECT_TRUE(state3.IsMouseButtonDown(MouseButton::Left));
	EXPECT_TRUE(state3.IsMouseButtonDown(MouseButton::Middle));
	EXPECT_TRUE(state3.IsMouseButtonDown(MouseButton::Right));
	EXPECT_TRUE(state3.IsMouseButtonDown(MouseButton::X1));
	EXPECT_TRUE(state3.IsMouseButtonDown(MouseButton::X2));
}

TEST(InputState, test_mouse_moved_events_update_position_and_accumulate_delta)
{
	InputState state;

	// Single MouseMoved event.
	{
		const Event events[] = {MakeMouseMoved(300, 400, 10, -5)};
		state.Update(events);
	}
	{
		const auto [px, py] = state.GetMousePosition();
		EXPECT_EQ(px, 300);
		EXPECT_EQ(py, 400);
	}
	{
		const auto [dx, dy] = state.GetMouseDelta();
		EXPECT_EQ(dx, 10);
		EXPECT_EQ(dy, -5);
	}

	// Multiple MouseMoved events: position = last, delta = sum.
	{
		const Event events[] = {
			MakeMouseMoved(100, 200, 3, 4),
			MakeMouseMoved(150, 250, 7, -2),
		};
		state.Update(events);
	}
	{
		const auto [px, py] = state.GetMousePosition();
		EXPECT_EQ(px, 150);
		EXPECT_EQ(py, 250);
	}
	{
		const auto [dx, dy] = state.GetMouseDelta();
		EXPECT_EQ(dx, 3 + 7);
		EXPECT_EQ(dy, 4 + (-2));
	}

	// No MouseMoved events: position retained, delta zero.
	{
		state.Update(std::span<const Event>{});
	}
	{
		const auto [px, py] = state.GetMousePosition();
		EXPECT_EQ(px, 150);
		EXPECT_EQ(py, 250);
	}
	{
		const auto [dx, dy] = state.GetMouseDelta();
		EXPECT_EQ(dx, 0);
		EXPECT_EQ(dy, 0);
	}
}

TEST(InputState, test_mouse_wheel_events_accumulate_scroll_delta)
{
	InputState state;

	// Single wheel event.
	{
		const Event events[] = {MakeMouseWheel(1.0f, -2.0f)};
		state.Update(events);
	}
	{
		const auto [sx, sy] = state.GetScrollDelta();
		EXPECT_FLOAT_EQ(sx, 1.0f);
		EXPECT_FLOAT_EQ(sy, -2.0f);
	}

	// Multiple wheel events accumulate.
	{
		const Event events[] = {
			MakeMouseWheel(0.5f, 1.5f),
			MakeMouseWheel(-0.5f, 0.5f),
		};
		state.Update(events);
	}
	{
		const auto [sx, sy] = state.GetScrollDelta();
		EXPECT_FLOAT_EQ(sx, 0.5f + (-0.5f));
		EXPECT_FLOAT_EQ(sy, 1.5f + 0.5f);
	}

	// No wheel events: delta is zero.
	{
		state.Update(std::span<const Event>{});
	}
	{
		const auto [sx, sy] = state.GetScrollDelta();
		EXPECT_FLOAT_EQ(sx, 0.0f);
		EXPECT_FLOAT_EQ(sy, 0.0f);
	}
}

TEST(InputState, test_get_mouse_position_returns_pair)
{
	InputState state;

	// Default-constructed: (0, 0).
	{
		const auto pos = state.GetMousePosition();
		EXPECT_EQ(pos.first, 0);
		EXPECT_EQ(pos.second, 0);
	}

	// After a MouseMoved event: reflects event coordinates.
	{
		const Event events[] = {MakeMouseMoved(42, 99, 0, 0)};
		state.Update(events);
	}
	{
		const auto pos = state.GetMousePosition();
		EXPECT_EQ(pos.first, 42);
		EXPECT_EQ(pos.second, 99);
	}

	// After Reset: back to (0, 0).
	state.Reset();
	{
		const auto pos = state.GetMousePosition();
		EXPECT_EQ(pos.first, 0);
		EXPECT_EQ(pos.second, 0);
	}
}

TEST(InputState, test_get_mouse_delta_returns_pair)
{
	InputState state;

	// Default: (0, 0).
	{
		const auto delta = state.GetMouseDelta();
		EXPECT_EQ(delta.first, 0);
		EXPECT_EQ(delta.second, 0);
	}

	// After two MouseMoved events: sum of deltas.
	{
		const Event events[] = {
			MakeMouseMoved(0, 0, 3, 7),
			MakeMouseMoved(0, 0, -1, 2),
		};
		state.Update(events);
	}
	{
		const auto delta = state.GetMouseDelta();
		EXPECT_EQ(delta.first, 3 + (-1));
		EXPECT_EQ(delta.second, 7 + 2);
	}
}

TEST(InputState, test_get_scroll_delta_returns_pair)
{
	InputState state;

	// Default: (0.0f, 0.0f).
	{
		const auto scroll = state.GetScrollDelta();
		EXPECT_FLOAT_EQ(scroll.first, 0.0f);
		EXPECT_FLOAT_EQ(scroll.second, 0.0f);
	}

	// After two MouseWheel events: sum.
	{
		const Event events[] = {
			MakeMouseWheel(1.25f, -0.75f),
			MakeMouseWheel(0.25f, 0.25f),
		};
		state.Update(events);
	}
	{
		const auto scroll = state.GetScrollDelta();
		EXPECT_FLOAT_EQ(scroll.first, 1.25f + 0.25f);
		EXPECT_FLOAT_EQ(scroll.second, -0.75f + 0.25f);
	}
}

TEST(InputState, test_reset_clears_all_state_to_default)
{
	InputState state;

	// Put state into a non-default condition.
	{
		const Event events[] = {
			MakeKeyDown(KeyCode::A),
			MakeMouseButtonDown(MouseButton::Left),
			MakeMouseMoved(50, 60, 10, 20),
			MakeMouseWheel(1.0f, 2.0f),
		};
		state.Update(events);
	}
	ASSERT_TRUE(state.IsKeyDown(KeyCode::A));
	ASSERT_TRUE(state.IsMouseButtonDown(MouseButton::Left));

	state.Reset();

	// All keys up.
	const auto keyCount = static_cast<std::size_t>(KeyCode::Count);
	for (std::size_t i = 0; i < keyCount; ++i)
	{
		const auto key = static_cast<KeyCode>(i);
		EXPECT_FALSE(state.IsKeyDown(key)) << "KeyCode index " << i;
		EXPECT_FALSE(state.WasKeyPressed(key)) << "KeyCode index " << i;
		EXPECT_FALSE(state.WasKeyReleased(key)) << "KeyCode index " << i;
	}

	// All mouse buttons up.
	EXPECT_FALSE(state.IsMouseButtonDown(MouseButton::Left));
	EXPECT_FALSE(state.IsMouseButtonDown(MouseButton::Middle));
	EXPECT_FALSE(state.IsMouseButtonDown(MouseButton::Right));
	EXPECT_FALSE(state.IsMouseButtonDown(MouseButton::X1));
	EXPECT_FALSE(state.IsMouseButtonDown(MouseButton::X2));

	// Position and deltas zeroed.
	{
		const auto [px, py] = state.GetMousePosition();
		EXPECT_EQ(px, 0);
		EXPECT_EQ(py, 0);
	}
	{
		const auto [dx, dy] = state.GetMouseDelta();
		EXPECT_EQ(dx, 0);
		EXPECT_EQ(dy, 0);
	}
	{
		const auto [sx, sy] = state.GetScrollDelta();
		EXPECT_FLOAT_EQ(sx, 0.0f);
		EXPECT_FLOAT_EQ(sy, 0.0f);
	}

	// Subsequent Update behaves as if freshly constructed.
	{
		const Event events[] = {MakeKeyDown(KeyCode::B)};
		state.Update(events);
	}
	EXPECT_TRUE(state.IsKeyDown(KeyCode::B));
	EXPECT_TRUE(state.WasKeyPressed(KeyCode::B)); // previous was up (reset)
}

TEST(InputState, test_query_methods_are_const_and_nodiscard)
{
	const InputState state;

	// All query methods are callable on a const instance.
	[[maybe_unused]] const bool kd = state.IsKeyDown(KeyCode::A);
	[[maybe_unused]] const bool wkp = state.WasKeyPressed(KeyCode::A);
	[[maybe_unused]] const bool wkr = state.WasKeyReleased(KeyCode::A);
	[[maybe_unused]] const bool mbd = state.IsMouseButtonDown(MouseButton::Left);
	[[maybe_unused]] const auto mp = state.GetMousePosition();
	[[maybe_unused]] const auto md = state.GetMouseDelta();
	[[maybe_unused]] const auto sd = state.GetScrollDelta();

	// KeyCode::Unknown returns false from all key queries.
	EXPECT_FALSE(state.IsKeyDown(KeyCode::Unknown));
	EXPECT_FALSE(state.WasKeyPressed(KeyCode::Unknown));
	EXPECT_FALSE(state.WasKeyReleased(KeyCode::Unknown));
}
