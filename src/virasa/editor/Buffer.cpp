#include "virasa/editor/Buffer.h"

#include <algorithm>

namespace virasa::editor
{
namespace
{
[[nodiscard]] bool IsContinuationByte(char value) noexcept
{
	return (static_cast<unsigned char>(value) & 0xC0u) == 0x80u;
}

[[nodiscard]] std::size_t ClampToSize(std::size_t value, std::size_t size) noexcept
{
	return std::min(value, size);
}

[[nodiscard]] std::size_t SnapToCodepointBoundaryAtOrBefore(
	const std::string& text, std::size_t byteIndex) noexcept
{
	std::size_t index = ClampToSize(byteIndex, text.size());

	while (index > 0u && index < text.size() && IsContinuationByte(text[index]))
	{
		--index;
	}

	return index;
}
} // namespace

void Buffer::Clear()
{
	_text.clear();
	_cursorByte = 0u;
}

void Buffer::SetText(std::string_view text)
{
	_text.assign(text.data(), text.size());
	_cursorByte = 0u;
}

std::string_view Buffer::GetText() const noexcept
{
	return std::string_view(_text);
}

std::size_t Buffer::GetCursorByte() const noexcept
{
	return _cursorByte;
}

void Buffer::SetCursorByte(std::size_t byteIndex)
{
	_cursorByte = SnapToCodepointBoundaryAtOrBefore(_text, byteIndex);
}

void Buffer::InsertAtCursor(std::string_view text)
{
	if (text.empty())
	{
		return;
	}

	_text.insert(_cursorByte, text.data(), text.size());
	_cursorByte += text.size();
}

void Buffer::EraseRange(std::size_t beginByte, std::size_t endByte)
{
	std::size_t begin = ClampToSize(beginByte, _text.size());
	std::size_t end = ClampToSize(endByte, _text.size());

	if (begin > end)
	{
		std::swap(begin, end);
	}

	if (begin == end)
	{
		return;
	}

	const std::size_t eraseCount = end - begin;

	_text.erase(begin, eraseCount);

	if (_cursorByte <= begin)
	{
	}
	else if (_cursorByte <= end)
	{
		_cursorByte = begin;
	}
	else
	{
		_cursorByte -= eraseCount;
	}

	_cursorByte = SnapToCodepointBoundaryAtOrBefore(_text, _cursorByte);
}

std::size_t Buffer::GetLineCount() const noexcept
{
	std::size_t lineCount = 1u;

	for (char value : _text)
	{
		if (value == '\n')
		{
			++lineCount;
		}
	}

	return lineCount;
}

std::size_t Buffer::GetLineStartByte(std::size_t lineIndex) const
{
	if (lineIndex == 0u)
	{
		return 0u;
	}

	std::size_t currentLine = 0u;

	for (std::size_t index = 0u; index < _text.size(); ++index)
	{
		if (_text[index] == '\n')
		{
			++currentLine;
			if (currentLine == lineIndex)
			{
				return index + 1u;
			}
		}
	}

	return _text.size();
}

std::size_t Buffer::GetLineEndByte(std::size_t lineIndex) const
{
	const std::size_t start = GetLineStartByte(lineIndex);
	if (start == _text.size())
	{
		return _text.size();
	}

	for (std::size_t index = start; index < _text.size(); ++index)
	{
		if (_text[index] == '\n')
		{
			return index;
		}
	}

	return _text.size();
}

std::size_t Buffer::GetCursorLine() const noexcept
{
	std::size_t line = 0u;
	const std::size_t limit = ClampToSize(_cursorByte, _text.size());

	for (std::size_t index = 0u; index < limit; ++index)
	{
		if (_text[index] == '\n')
		{
			++line;
		}
	}

	return line;
}

std::size_t Buffer::GetCursorColumnByte() const noexcept
{
	const std::size_t line = GetCursorLine();
	const std::size_t lineStart = GetLineStartByte(line);
	return _cursorByte - lineStart;
}

std::size_t Buffer::PrevCodepointBoundary(std::size_t byteIndex) const noexcept
{
	const std::size_t clamped = ClampToSize(byteIndex, _text.size());
	if (clamped == 0u)
	{
		return 0u;
	}

	std::size_t index = clamped - 1u;
	while (index > 0u && IsContinuationByte(_text[index]))
	{
		--index;
	}

	return index;
}

std::size_t Buffer::NextCodepointBoundary(std::size_t byteIndex) const noexcept
{
	const std::size_t clamped = ClampToSize(byteIndex, _text.size());
	if (clamped == _text.size())
	{
		return _text.size();
	}

	std::size_t index = clamped + 1u;
	while (index < _text.size() && IsContinuationByte(_text[index]))
	{
		++index;
	}

	return index;
}

} // namespace virasa::editor
