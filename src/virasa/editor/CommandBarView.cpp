#include "virasa/editor/CommandBarView.h"

namespace virasa::editor
{

std::string_view CommandBarView::GetText() const noexcept
{
	return _text;
}

std::size_t CommandBarView::GetCursorByte() const noexcept
{
	return _cursorByte;
}

void CommandBarView::Clear() noexcept
{
	_text.clear();
	_cursorByte = 0u;
}

void CommandBarView::SetText(std::string_view text)
{
	_text.assign(text.data(), text.size());
	_cursorByte = _text.size();
}

CommandBarKeyResult CommandBarView::HandleKey(virasa::KeyCode key)
{
	switch (key)
	{
		case virasa::KeyCode::Backspace:
		{
			if (_cursorByte == 0u)
			{
				return CommandBarKeyResult::Consumed;
			}
			// Walk backward to find the start of the preceding codepoint.
			std::size_t v = _cursorByte - 1u;
			while (v > 0u && (static_cast<uint8_t>(_text[v]) & 0xC0u) == 0x80u)
			{
				--v;
			}
			_text.erase(v, _cursorByte - v);
			_cursorByte = v;
			return CommandBarKeyResult::Consumed;
		}
		case virasa::KeyCode::Enter:
		{
			return CommandBarKeyResult::Submitted;
		}
		case virasa::KeyCode::Escape:
		{
			Clear();
			return CommandBarKeyResult::Consumed;
		}
		default:
		{
			return CommandBarKeyResult::Consumed;
		}
	}
}

CommandBarKeyResult CommandBarView::HandleTextInput(std::string_view utf8)
{
	if (!utf8.empty())
	{
		_text.insert(_cursorByte, utf8.data(), utf8.size());
		_cursorByte += utf8.size();
	}
	return CommandBarKeyResult::Consumed;
}

CommandResult CommandBarView::Submit() noexcept
{
	CommandResult result = CommandResult::None;

	if (_text == ":ide")
	{
		result = CommandResult::OpenEditor;
	}
	else if (_text == ":tree")
	{
		result = CommandResult::OpenHierarchy;
	}
	else if (_text == ":q")
	{
		result = CommandResult::Quit;
	}
	else if (_text.empty() || _text == ":")
	{
		result = CommandResult::Close;
	}
	else
	{
		result = CommandResult::None;
	}

	_text.clear();
	_cursorByte = 0u;

	return result;
}

void CommandBarView::Render(
	virasa::ui::DrawList& out,
	const virasa::ui::FontAtlas& atlas,
	uint32_t windowWidth,
	uint32_t windowHeight) const
{
	_panel.Render(out, _text, _cursorByte, atlas, windowWidth, windowHeight);
}

const virasa::ui::CommandBar& CommandBarView::GetPanel() const noexcept
{
	return _panel;
}

virasa::ui::CommandBar& CommandBarView::GetPanel() noexcept
{
	return _panel;
}

} // namespace virasa::editor
