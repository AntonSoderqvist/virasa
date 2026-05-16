#ifndef VIRASA_WINDOW_INPUTSTATE_H
#define VIRASA_WINDOW_INPUTSTATE_H

#include <cstdint>
#include <span>
#include <utility>

#include "virasa/window/Events.h"

namespace virasa
{

/**
 * @brief Tracks keyboard, mouse button, mouse position, and scroll state across frames.
 *
 * InputState is updated once per frame via Update(events). After each Update,
 * the query methods reflect the new frame's state. A default-constructed InputState
 * reports all keys and buttons as up and all deltas as zero.
 */
class InputState final
{
	public:
	/**
	 * @brief Advances the InputState by one frame, applying the given events in order.
	 * @param events A span of Event objects produced by the platform layer for this frame.
	 */
	void Update(std::span<const Event> events);

	/**
	 * @brief Returns true if the given key is currently held down.
	 * @param key The KeyCode to query.
	 * @return True if the key is down this frame.
	 */
	[[nodiscard]] bool IsKeyDown(KeyCode key) const;

	/**
	 * @brief Returns true if the given key transitioned from up to down this frame.
	 * @param key The KeyCode to query.
	 * @return True if the key was pressed (up last frame, down this frame).
	 */
	[[nodiscard]] bool WasKeyPressed(KeyCode key) const;

	/**
	 * @brief Returns true if the given key transitioned from down to up this frame.
	 * @param key The KeyCode to query.
	 * @return True if the key was released (down last frame, up this frame).
	 */
	[[nodiscard]] bool WasKeyReleased(KeyCode key) const;

	/**
	 * @brief Returns true if the given mouse button is currently held down.
	 * @param button The MouseButton to query.
	 * @return True if the button is down this frame.
	 */
	[[nodiscard]] bool IsMouseButtonDown(MouseButton button) const;

	/**
	 * @brief Returns the current mouse position in window coordinates.
	 * @return A pair (x, y) in window coordinates.
	 */
	[[nodiscard]] std::pair<int32_t, int32_t> GetMousePosition() const;

	/**
	 * @brief Returns the accumulated mouse delta for the current frame.
	 * @return A pair (deltaX, deltaY) summed from all MouseMoved events this frame.
	 */
	[[nodiscard]] std::pair<int32_t, int32_t> GetMouseDelta() const;

	/**
	 * @brief Returns the accumulated scroll delta for the current frame.
	 * @return A pair (scrollX, scrollY) summed from all MouseWheel events this frame.
	 */
	[[nodiscard]] std::pair<float, float> GetScrollDelta() const;

	/**
	 * @brief Resets all state to the same observable state as a default-constructed InputState.
	 */
	void Reset();

	private:
	static constexpr std::size_t kKeyCount = static_cast<std::size_t>(KeyCode::Count);
	static constexpr std::size_t kMouseButtonCount = 5u; // Left, Middle, Right, X1, X2

	// Current-frame key state (indexed by KeyCode cast to size_t)
	bool _keyCurrent[kKeyCount] = {};
	// Previous-frame key state
	bool _keyPrevious[kKeyCount] = {};

	// Current-frame mouse button state (indexed by MouseButton cast to size_t)
	bool _mouseCurrent[kMouseButtonCount] = {};

	int32_t _mouseX = 0;
	int32_t _mouseY = 0;
	int32_t _mouseDeltaX = 0;
	int32_t _mouseDeltaY = 0;
	float _scrollX = 0.0f;
	float _scrollY = 0.0f;
};

} // namespace virasa

#endif // VIRASA_WINDOW_INPUTSTATE_H
