#ifndef VIRASA_EDITOR_COMMAND_BAR_VIEW_H
#define VIRASA_EDITOR_COMMAND_BAR_VIEW_H

#include "virasa/ui/CommandBar.h"
#include "virasa/ui/Types.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/window/Events.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace virasa::editor
{

/**
 * @brief Result returned by key/text-input handlers on CommandBarView.
 *
 * Consumed indicates the event was handled and should not propagate further.
 * Submitted indicates an Enter key completed the command line; the caller
 * should invoke Submit() to obtain the CommandResult.
 */
enum class CommandBarKeyResult : uint8_t
{
	Consumed,
	Submitted
};

/**
 * @brief Result returned by Submit() after parsing the command line.
 *
 * None          - no known command matched; leave state unchanged.
 * OpenEditor    - ":ide" command; open (or toggle) the editor view.
 * OpenHierarchy - ":tree" command; open (or toggle) the hierarchy view.
 * Close         - empty command ("" or ":"); close the right panel.
 * Quit          - ":q" command; begin application shutdown.
 * LoadModel     - ":load <path>" command; load the submitted model path.
 */
enum class CommandResult : uint8_t
{
	None,
	OpenEditor,
	OpenHierarchy,
	Close,
	Quit,
	LoadModel
};

/**
 * @brief Owns the command-bar text buffer, cursor position, and stateless
 *        render panel for the editor command line.
 *
 * CommandBarView is a final class that holds:
 *   - a virasa::ui::CommandBar (_panel) for stateless rendering,
 *   - a std::string (_text) for the UTF-8 bytes being edited,
 *   - a std::string (_argument) for the most recently submitted argument,
 *   - a std::size_t (_cursorByte) for the byte-offset insertion cursor.
 *
 * It has no Vulkan or ECS dependency and owns no external resources.
 */
class CommandBarView final
{
public:
	/// @brief Default constructor. _text is empty, _cursorByte is 0.
	CommandBarView() = default;

	CommandBarView(const CommandBarView&) = default;
	CommandBarView(CommandBarView&&) = default;
	CommandBarView& operator=(const CommandBarView&) = default;
	CommandBarView& operator=(CommandBarView&&) = default;
	~CommandBarView() = default;

	/**
	 * @brief Returns a view over the internal text buffer.
	 * @return std::string_view valid until the next mutating operation.
	 */
	[[nodiscard]] std::string_view GetText() const noexcept;

	/**
	 * @brief Returns the current cursor byte offset into the text buffer.
	 * @return Byte offset in [0, GetText().size()].
	 */
	[[nodiscard]] std::size_t GetCursorByte() const noexcept;

	/**
	 * @brief Returns a view over the most recently submitted argument.
	 * @return std::string_view valid until the next mutating operation.
	 */
	[[nodiscard]] std::string_view GetSubmittedArgument() const noexcept;

	/**
	 * @brief Empties the text buffer, submitted argument, and resets the cursor to 0.
	 */
	void Clear() noexcept;

	/**
	 * @brief Replaces the text buffer with a copy of the supplied bytes
	 *        and places the cursor at the end.
	 * @param text UTF-8 bytes to copy verbatim.
	 */
	void SetText(std::string_view text);

	/**
	 * @brief Handles a non-printable key event.
	 * @param key The physical key code.
	 * @return CommandBarKeyResult::Submitted on Enter, Consumed otherwise.
	 */
	CommandBarKeyResult HandleKey(virasa::KeyCode key);

	/**
	 * @brief Inserts UTF-8 bytes at the current cursor position.
	 * @param utf8 Bytes to insert verbatim.
	 * @return CommandBarKeyResult::Consumed always.
	 */
	CommandBarKeyResult HandleTextInput(std::string_view utf8);

	/**
	 * @brief Parses the current text buffer and returns the matching command.
	 *
	 * Clears the text buffer and resets the cursor after parsing. When the
	 * parsed command is ":load <path>", stores the submitted path in the
	 * internal argument buffer.
	 * @return The CommandResult corresponding to the current text.
	 */
	CommandResult Submit();

	/**
	 * @brief Delegates rendering to the owned CommandBar.
	 * @param out       DrawList to append commands into.
	 * @param atlas     Font atlas used for glyph metrics.
	 * @param windowWidth  Framebuffer width in pixels.
	 * @param windowHeight Framebuffer height in pixels.
	 */
	void Render(
		virasa::ui::DrawList& out,
		const virasa::ui::FontAtlas& atlas,
		uint32_t windowWidth,
		uint32_t windowHeight) const;

	/**
	 * @brief Returns a const reference to the underlying CommandBar.
	 * @return Const reference to _panel.
	 */
	[[nodiscard]] const virasa::ui::CommandBar& GetPanel() const noexcept;

	/**
	 * @brief Returns a mutable reference to the underlying CommandBar.
	 * @return Mutable reference to _panel.
	 */
	[[nodiscard]] virasa::ui::CommandBar& GetPanel() noexcept;

private:
	virasa::ui::CommandBar _panel = {};
	std::string _text = {};
	std::string _argument = {};
	std::size_t _cursorByte = 0u;
};

} // namespace virasa::editor

#endif // VIRASA_EDITOR_COMMAND_BAR_VIEW_H
