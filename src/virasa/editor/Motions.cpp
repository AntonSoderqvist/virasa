#include "virasa/editor/Motions.h"

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace virasa
{
namespace editor
{
namespace
{
	[[nodiscard]] bool IsWhitespaceByte(char byte) noexcept
	{
		return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r';
	}

	[[nodiscard]] bool IsWordByte(char byte) noexcept
	{
		return !IsWhitespaceByte(byte);
	}
}

Mode MotionState::GetMode() const noexcept
{
	return _mode;
}

void MotionState::SetMode(Mode mode) noexcept
{
	_mode = mode;
	Reset();
}

void MotionState::Reset() noexcept
{
	_pendingCount = 0u;
	_pendingOperator = PendingOperator::None;
	_pendingG = false;
}

KeyResult MotionState::HandleKey(virasa::KeyCode key, virasa::editor::Buffer& buffer)
{
	(void)buffer;

	switch (_mode)
	{
	case Mode::Normal:
		if (key == virasa::KeyCode::Escape)
		{
			Reset();
		}
		return KeyResult::Consumed;

	case Mode::Insert:
		if (key == virasa::KeyCode::Escape)
		{
			SetMode(Mode::Normal);
			return KeyResult::Consumed;
		}
		if (key == virasa::KeyCode::Enter)
		{
			buffer.InsertAtCursor("\n");
			return KeyResult::Consumed;
		}
		if (key == virasa::KeyCode::Backspace)
		{
			const std::size_t cursorByte = buffer.GetCursorByte();
			if (cursorByte > 0)
			{
				buffer.EraseRange(buffer.PrevCodepointBoundary(cursorByte), cursorByte);
			}
			return KeyResult::Consumed;
		}
		return KeyResult::Consumed;
	}

	return KeyResult::Consumed;
}

KeyResult MotionState::HandleTextInput(std::string_view utf8, virasa::editor::Buffer& buffer)
{
	if (_mode == Mode::Insert)
	{
		buffer.InsertAtCursor(utf8);
		return KeyResult::Consumed;
	}

	for (std::size_t i = 0; i < utf8.size();)
	{
		const unsigned char lead = static_cast<unsigned char>(utf8[i]);
		std::size_t codepointSize = 1;
		if ((lead & 0x80u) == 0x00u)
		{
			codepointSize = 1;
		}
		else if ((lead & 0xE0u) == 0xC0u)
		{
			codepointSize = 2;
		}
		else if ((lead & 0xF0u) == 0xE0u)
		{
			codepointSize = 3;
		}
		else if ((lead & 0xF8u) == 0xF0u)
		{
			codepointSize = 4;
		}
		if (i + codepointSize > utf8.size())
		{
			codepointSize = utf8.size() - i;
		}

		const std::string_view codepoint = utf8.substr(i, codepointSize);
		i += codepointSize;

		if (_pendingG && codepoint != "g")
		{
			_pendingG = false;
		}
		if (_pendingOperator == PendingOperator::Delete && codepoint != "d")
		{
			_pendingOperator = PendingOperator::None;
		}

		if (codepoint == "h")
		{
			const std::size_t cursorLine = buffer.GetCursorLine();
			const std::size_t cursorByte = buffer.GetCursorByte();
			const std::size_t candidate = buffer.PrevCodepointBoundary(cursorByte);
			if (candidate >= buffer.GetLineStartByte(cursorLine))
			{
				buffer.SetCursorByte(candidate);
			}
			Reset();
			continue;
		}

		if (codepoint == "l")
		{
			const std::size_t cursorLine = buffer.GetCursorLine();
			const std::size_t cursorByte = buffer.GetCursorByte();
			const std::size_t candidate = buffer.NextCodepointBoundary(cursorByte);
			if (candidate <= buffer.GetLineEndByte(cursorLine))
			{
				buffer.SetCursorByte(candidate);
			}
			Reset();
			continue;
		}

		if (codepoint == "j")
		{
			const std::size_t cursorLine = buffer.GetCursorLine();
			const std::size_t lineCount = buffer.GetLineCount();
			if (cursorLine + 1 < lineCount)
			{
				const std::size_t col = buffer.GetCursorColumnByte();
				const std::size_t nextLine = cursorLine + 1;
				const std::size_t candidate = std::min(
					buffer.GetLineStartByte(nextLine) + col,
					buffer.GetLineEndByte(nextLine)
				);
				buffer.SetCursorByte(candidate);
			}
			Reset();
			continue;
		}

		if (codepoint == "k")
		{
			const std::size_t cursorLine = buffer.GetCursorLine();
			if (cursorLine > 0)
			{
				const std::size_t col = buffer.GetCursorColumnByte();
				const std::size_t prevLine = cursorLine - 1;
				const std::size_t candidate = std::min(
					buffer.GetLineStartByte(prevLine) + col,
					buffer.GetLineEndByte(prevLine)
				);
				buffer.SetCursorByte(candidate);
			}
			Reset();
			continue;
		}

		if (codepoint == "0")
		{
			buffer.SetCursorByte(buffer.GetLineStartByte(buffer.GetCursorLine()));
			Reset();
			continue;
		}

		if (codepoint == "$")
		{
			const std::size_t cursorLine = buffer.GetCursorLine();
			const std::size_t lineStart = buffer.GetLineStartByte(cursorLine);
			const std::size_t lineEnd = buffer.GetLineEndByte(cursorLine);
			if (lineEnd == lineStart)
			{
				buffer.SetCursorByte(lineStart);
			}
			else
			{
				buffer.SetCursorByte(buffer.PrevCodepointBoundary(lineEnd));
			}
			Reset();
			continue;
		}

		if (codepoint == "g")
		{
			if (!_pendingG)
			{
				_pendingG = true;
				continue;
			}

			buffer.SetCursorByte(0);
			Reset();
			continue;
		}

		if (codepoint == "G")
		{
			buffer.SetCursorByte(buffer.GetLineStartByte(buffer.GetLineCount() - 1));
			Reset();
			continue;
		}

		if (codepoint == "i")
		{
			_mode = Mode::Insert;
			Reset();
			continue;
		}

		if (codepoint == "a")
		{
			const std::size_t cursorLine = buffer.GetCursorLine();
			const std::size_t cursorByte = buffer.GetCursorByte();
			const std::size_t candidate = buffer.NextCodepointBoundary(cursorByte);
			if (candidate <= buffer.GetLineEndByte(cursorLine))
			{
				buffer.SetCursorByte(candidate);
			}
			_mode = Mode::Insert;
			Reset();
			continue;
		}

		if (codepoint == "o")
		{
			const std::size_t lineEnd = buffer.GetLineEndByte(buffer.GetCursorLine());
			buffer.SetCursorByte(lineEnd);
			buffer.InsertAtCursor("\n");
			_mode = Mode::Insert;
			Reset();
			continue;
		}

		if (codepoint == "O")
		{
			const std::size_t lineStart = buffer.GetLineStartByte(buffer.GetCursorLine());
			buffer.SetCursorByte(lineStart);
			buffer.InsertAtCursor("\n");
			buffer.SetCursorByte(lineStart);
			_mode = Mode::Insert;
			Reset();
			continue;
		}

		if (codepoint == "x")
		{
			const std::size_t begin = buffer.GetCursorByte();
			const std::size_t end = buffer.NextCodepointBoundary(begin);
			if (end <= buffer.GetLineEndByte(buffer.GetCursorLine()) && begin != end)
			{
				buffer.EraseRange(begin, end);
			}
			Reset();
			continue;
		}

		if (codepoint == "d")
		{
			if (_pendingOperator == PendingOperator::None)
			{
				_pendingOperator = PendingOperator::Delete;
				continue;
			}

			const std::size_t line = buffer.GetCursorLine();
			const std::size_t lineStart = buffer.GetLineStartByte(line);
			std::size_t end = buffer.GetLineEndByte(line);
			if (line + 1 < buffer.GetLineCount())
			{
				end = buffer.GetLineStartByte(line + 1);
			}
			buffer.EraseRange(lineStart, end);
			Reset();
			continue;
		}

		if (codepoint == "w")
		{
			const std::string_view text = buffer.GetText();
			std::size_t index = buffer.GetCursorByte();
			while (index < text.size() && IsWordByte(text[index]))
			{
				++index;
			}
			while (index < text.size() && IsWhitespaceByte(text[index]))
			{
				++index;
			}
			buffer.SetCursorByte(index);
			Reset();
			continue;
		}

		if (codepoint == "b")
		{
			const std::string_view text = buffer.GetText();
			const std::size_t c0 = buffer.GetCursorByte();
			if (c0 > 0)
			{
				std::size_t index = c0 - 1;
				while (index > 0 && IsWhitespaceByte(text[index]))
				{
					--index;
				}
				while (index > 0 && IsWordByte(text[index]))
				{
					if (IsWhitespaceByte(text[index - 1]))
					{
						break;
					}
					--index;
				}
				buffer.SetCursorByte(index);
			}
			Reset();
			continue;
		}

		if (codepoint == ":")
		{
			Reset();
			return KeyResult::RequestCommandBar;
		}
	}

	return KeyResult::Consumed;
}

}
}
