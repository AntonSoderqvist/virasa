#ifndef VIRASA_WINDOW_INPUTSTATE_H
#define VIRASA_WINDOW_INPUTSTATE_H

#include "virasa/window/events.h"

#include <cstdint>
#include <span>
#include <utility>

namespace virasa
{

/**
 * @brief Tracks keyboard, mouse button, mouse position, and scroll state across frames.
 *
 * InputState is updated once per frame via Update(events). Query methods reflect the
 * state after the most recent Update call. A default-constructed InputState reports
 * all keys and buttons as up and all deltas as zero.
 */
class InputState final
{
public:
	/**
	 * @brief Default-constructs an InputState with all keys/buttons up and all deltas zero.
	 */
	InputState() = default;

	InputState(const InputState&) = default;
	InputState(InputState&&) = default;
	InputState& operator=(const InputState&) = default;
	InputState& operator=(InputState&&) = default;
	~InputState() = default;

	/**
	 * @brief Advances the InputState by one frame, consuming the supplied events.
	 *
	 * Snapshots current state as previous-frame state, clears per-frame deltas,
	 * then applies each event in order.
	 *
	 * @param events Span of events produced for this frame by the platform layer.
	 */
	void Update(std::span<const Event> events);

	/**
	 * @brief Queries whether the given key is currently held down.
	 * @param key The key code to query.
	 * @return true if the key is currently down, false otherwise.
	 */
	[[nodiscard]] bool IsKeyDown(KeyCode key) const;

	/**
	 * @brief Queries whether the given key transitioned from up to down this frame.
	 * @param key The key code to query.
	 * @return true if the key was pressed this frame (up -> down edge).
	 */
	[[nodiscard]] bool WasKeyPressed(KeyCode key) const;

	/**
	 * @brief Queries whether the given key transitioned from down to up this frame.
	 * @param key The key code to query.
	 * @return true if the key was released this frame (down -> up edge).
	 */
	[[nodiscard]] bool WasKeyReleased(KeyCode key) const;

	/**
	 * @brief Queries whether the given mouse button is currently held down.
	 * @param button The mouse button to query.
	 * @return true if the button is currently down, false otherwise.
	 */
	[[nodiscard]] bool IsMouseButtonDown(MouseButton button) const;

	/**
	 * @brief Returns the current mouse position in window coordinates.
	 * @return Pair of (x, y) in window coordinates.
	 */
	[[nodiscard]] std::pair<int32_t, int32_t> GetMousePosition() const;

	/**
	 * @brief Returns the accumulated mouse delta for the current frame.
	 * @return Pair of (deltaX, deltaY) accumulated from all MouseMoved events this frame.
	 */
	[[nodiscard]] std::pair<int32_t, int32_t> GetMouseDelta() const;

	/**
	 * @brief Returns the accumulated scroll delta for the current frame.
	 * @return Pair of (scrollX, scrollY) accumulated from all MouseWheel events this frame.
	 */
	[[nodiscard]] std::pair<float, float> GetScrollDelta() const;

	/**
	 * @brief Resets all state to the same observable state as a default-constructed InputState.
	 *
	 * All keys and buttons are set to up (both current and previous-frame state).
	 * Mouse position, delta, and scroll delta are all zeroed.
	 */
	void Reset();

private:
	static constexpr int kKeyCount = static_cast<int>(KeyCode::Count);
	static constexpr int kMouseButtonCount = 5; // Left, Middle, Right, X1, X2

	// Current-frame key state (indexed by KeyCode underlying value)
	bool _currentKeys[kKeyCount] = {};
	// Previous-frame key state snapshot
	bool _previousKeys[kKeyCount] = {};

	// Current mouse button state (indexed by MouseButton underlying value)
	bool _currentMouseButtons[kMouseButtonCount] = {};

	// Mouse position in window coordinates
	int32_t _mouseX = 0;
	int32_t _mouseY = 0;

	// Per-frame mouse delta (cleared each Update)
	int32_t _mouseDeltaX = 0;
	int32_t _mouseDeltaY = 0;

	// Per-frame scroll delta (cleared each Update)
	float _scrollDeltaX = 0.0f;
	float _scrollDeltaY = 0.0f;
};

} // namespace virasa

#endif // VIRASA_WINDOW_INPUTSTATE_H
