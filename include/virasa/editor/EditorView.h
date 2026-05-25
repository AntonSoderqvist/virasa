#ifndef VIRASA_EDITOR_EDITORVIEW_H
#define VIRASA_EDITOR_EDITORVIEW_H

#include <cstdint>
#include <string_view>
#include "virasa/editor/Buffer.h"
#include "virasa/editor/Motions.h"
#include "virasa/ui/TextPanel.h"
#include "virasa/ui/Types.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/window/Events.h"

namespace virasa::editor
{

/**
 * @brief Result returned by EditorView key and text-input handlers.
 *
 * Consumed indicates the EditorView handled the input and the caller should
 * not propagate it further. RequestCommandBar indicates the input was a
 * request to transfer focus to the command bar; the event is also consumed
 * from the EditorView's perspective.
 */
enum class EditorViewKeyResult : uint8_t
{
	Consumed,
	RequestCommandBar
};

/**
 * @brief Owns a Buffer, a MotionState, and a TextPanel and wires them together
 *        into a self-contained vi-like text editor view.
 *
 * EditorView is the primary entry point for embedding an editor pane in the
 * UI. Callers feed keyboard and text-input events through HandleKey /
 * HandleTextInput and drive rendering through Render. EditorView owns no
 * external resources and has no Vulkan or ECS dependency.
 */
class EditorView final
{
public:
	/**
	 * @brief Default-constructs an EditorView with an empty buffer, Normal mode,
	 *        and default panel configuration.
	 */
	EditorView() = default;

	EditorView(const EditorView&) = default;
	EditorView(EditorView&&) = default;
	EditorView& operator=(const EditorView&) = default;
	EditorView& operator=(EditorView&&) = default;
	~EditorView() = default;

	/**
	 * @brief Returns a mutable reference to the owned Buffer.
	 * @return Mutable reference to _buffer.
	 */
	[[nodiscard]] virasa::editor::Buffer& GetBuffer() noexcept;

	/**
	 * @brief Returns a const reference to the owned Buffer.
	 * @return Const reference to _buffer.
	 */
	[[nodiscard]] const virasa::editor::Buffer& GetBuffer() const noexcept;

	/**
	 * @brief Returns a mutable reference to the owned MotionState.
	 * @return Mutable reference to _motionState.
	 */
	[[nodiscard]] virasa::editor::MotionState& GetMotionState() noexcept;

	/**
	 * @brief Returns a const reference to the owned MotionState.
	 * @return Const reference to _motionState.
	 */
	[[nodiscard]] const virasa::editor::MotionState& GetMotionState() const noexcept;

	/**
	 * @brief Returns a mutable reference to the owned TextPanel.
	 * @return Mutable reference to _panel.
	 */
	[[nodiscard]] virasa::ui::TextPanel& GetPanel() noexcept;

	/**
	 * @brief Returns a const reference to the owned TextPanel.
	 * @return Const reference to _panel.
	 */
	[[nodiscard]] const virasa::ui::TextPanel& GetPanel() const noexcept;

	/**
	 * @brief Forwards a key event to the MotionState and translates the result.
	 * @param key The key code of the pressed key.
	 * @return EditorViewKeyResult::Consumed or EditorViewKeyResult::RequestCommandBar.
	 */
	EditorViewKeyResult HandleKey(virasa::KeyCode key);

	/**
	 * @brief Forwards a text-input event to the MotionState and translates the result.
	 * @param utf8 UTF-8 encoded text produced by the platform's text-input system.
	 * @return EditorViewKeyResult::Consumed or EditorViewKeyResult::RequestCommandBar.
	 */
	EditorViewKeyResult HandleTextInput(std::string_view utf8);

	/**
	 * @brief Renders the editor view into the supplied DrawList.
	 * @param out      DrawList to append draw commands into.
	 * @param atlas    Font atlas used for text rendering.
	 * @param x        Left edge of the panel in framebuffer pixels.
	 * @param y        Top edge of the panel in framebuffer pixels.
	 * @param width    Width of the panel in framebuffer pixels.
	 * @param height   Height of the panel in framebuffer pixels.
	 */
	void Render(
		virasa::ui::DrawList& out,
		const virasa::ui::FontAtlas& atlas,
		float x,
		float y,
		float width,
		float height) const;

private:
	virasa::editor::Buffer _buffer = {};
	virasa::editor::MotionState _motionState = {};
	virasa::ui::TextPanel _panel = {};
};

} // namespace virasa::editor

#endif // VIRASA_EDITOR_EDITORVIEW_H
