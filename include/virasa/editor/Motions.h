#ifndef VIRASA_EDITOR_MOTIONS_H
#define VIRASA_EDITOR_MOTIONS_H

#include <cstdint>
#include <string_view>

#include "virasa/editor/Buffer.h"
#include "virasa/window/Events.h"

namespace virasa
{
namespace editor
{

/**
 * @brief Editing mode for motion handling.
 */
enum class Mode : uint8_t
{
	Normal,
	Insert
};

/**
 * @brief Result of handling a key or text input event.
 */
enum class KeyResult : uint8_t
{
	Consumed,
	RequestCommandBar
};

/**
 * @brief Pending operator awaiting completion by a motion or repeated key.
 */
enum class PendingOperator : uint8_t
{
	None,
	Delete
};

/**
 * @brief Handles modal editor motions and text insertion.
 */
class MotionState final
{
public:
	/**
	 * @brief Get the current editing mode.
	 * @return The current mode.
	 */
	[[nodiscard]] Mode GetMode() const noexcept;

	/**
	 * @brief Set the current editing mode and clear pending state.
	 * @param mode The mode to enter.
	 */
	void SetMode(Mode mode) noexcept;

	/**
	 * @brief Clear pending motion state without changing the current mode.
	 */
	void Reset() noexcept;

	/**
	 * @brief Handle a non-printable key event.
	 * @param key The physical key code.
	 * @param buffer The buffer to operate on.
	 * @return The handling result.
	 */
	[[nodiscard]] KeyResult HandleKey(virasa::KeyCode key, virasa::editor::Buffer& buffer);

	/**
	 * @brief Handle printable UTF-8 text input.
	 * @param utf8 The UTF-8 bytes produced by text input.
	 * @param buffer The buffer to operate on.
	 * @return The handling result.
	 */
	[[nodiscard]] KeyResult HandleTextInput(std::string_view utf8, virasa::editor::Buffer& buffer);

private:
	Mode _mode = Mode::Normal;
	uint32_t _pendingCount = 0u;
	PendingOperator _pendingOperator = PendingOperator::None;
	bool _pendingG = false;
};

}
}

#endif
