#include "virasa/editor/EditorView.h"

namespace virasa::editor
{

virasa::editor::Buffer& EditorView::GetBuffer() noexcept
{
	return _buffer;
}

const virasa::editor::Buffer& EditorView::GetBuffer() const noexcept
{
	return _buffer;
}

virasa::editor::MotionState& EditorView::GetMotionState() noexcept
{
	return _motionState;
}

const virasa::editor::MotionState& EditorView::GetMotionState() const noexcept
{
	return _motionState;
}

virasa::ui::TextPanel& EditorView::GetPanel() noexcept
{
	return _panel;
}

const virasa::ui::TextPanel& EditorView::GetPanel() const noexcept
{
	return _panel;
}

EditorViewKeyResult EditorView::HandleKey(virasa::KeyCode key)
{
	virasa::editor::KeyResult result = _motionState.HandleKey(key, _buffer);
	if (result == virasa::editor::KeyResult::RequestCommandBar)
	{
		return EditorViewKeyResult::RequestCommandBar;
	}
	return EditorViewKeyResult::Consumed;
}

EditorViewKeyResult EditorView::HandleTextInput(std::string_view utf8)
{
	virasa::editor::KeyResult result = _motionState.HandleTextInput(utf8, _buffer);
	if (result == virasa::editor::KeyResult::RequestCommandBar)
	{
		return EditorViewKeyResult::RequestCommandBar;
	}
	return EditorViewKeyResult::Consumed;
}

void EditorView::Render(
	virasa::ui::DrawList& out,
	const virasa::ui::FontAtlas& atlas,
	float x,
	float y,
	float width,
	float height) const
{
	_panel.Render(out, _buffer.GetText(), x, y, width, height, atlas);
}

} // namespace virasa::editor
