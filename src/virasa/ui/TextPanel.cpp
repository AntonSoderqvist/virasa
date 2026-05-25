#include "virasa/ui/TextPanel.h"

#include <string>

namespace virasa
{
namespace ui
{

void TextPanel::SetConfig(const TextPanelConfig& config) noexcept
{
	_config = config;
}

const TextPanelConfig& TextPanel::GetConfig() const noexcept
{
	return _config;
}

void TextPanel::Render(DrawList& out, std::string_view text, float x, float y, float width,
	float height, const FontAtlas& atlas, CursorStyle cursorStyle, std::size_t cursorByte) const
{
	// Background quad
	out.AddQuad(QuadCommand{x, y, width, height, _config.background});

	const float lineHeight = atlas.GetAscender() - atlas.GetDescender();
	const float advance = lineHeight * _config.lineSpacing;
	const float textX = x + _config.paddingX;
	const float baseY = y + _config.paddingY + atlas.GetAscender();

	// Clamp cursorByte to text.size()
	if (cursorByte > text.size())
	{
		cursorByte = text.size();
	}

	// Build logical lines: each entry is the emitted (non-\r) bytes of that line.
	// We also track, for each logical line, the byte offset in text where the line starts
	// (including \r bytes, so we can map cursorByte to a line).
	struct LineInfo
	{
		std::string bytes;       // emitted bytes (\r stripped)
		std::size_t textStart;   // byte offset in text of first byte of this line (after \n)
	};

	std::vector<LineInfo> lines;
	lines.reserve(32);

	{
		LineInfo current;
		current.textStart = 0;
		current.bytes.reserve(text.size());

		for (std::size_t i = 0; i < text.size(); ++i)
		{
			const char ch = text[i];
			if (ch == '\r')
			{
				continue;
			}
			if (ch == '\n')
			{
				lines.push_back(std::move(current));
				current = LineInfo{};
				current.textStart = i + 1;
				continue;
			}
			current.bytes.push_back(ch);
		}
		lines.push_back(std::move(current));
	}

	// Emit text commands for non-empty lines
	for (std::size_t k = 0; k < lines.size(); ++k)
	{
		const LineInfo& line = lines[k];
		if (!line.bytes.empty())
		{
			out.AddText(
				textX,
				baseY + static_cast<float>(k) * advance,
				std::string_view(line.bytes),
				_config.foreground);
		}
	}

	// Cursor quad
	if (cursorStyle == CursorStyle::None)
	{
		return;
	}

	// Determine which logical line the cursor is on and the column byte count.
	// We walk the raw text bytes to find the line boundary that contains cursorByte.
	std::size_t kCursor = 0;
	std::size_t columnBytes = 0;
	{
		// Find the logical line index: the line whose [textStart, next \n) contains cursorByte.
		// lines[k].textStart is the raw text offset after the k-th \n.
		// The line ends at the raw offset of the (k+1)-th \n, or text.size().
		// cursorByte is in line k if textStart[k] <= cursorByte <= textStart[k+1]-1
		// (where textStart[k+1]-1 is the offset of the \n terminating line k).
		// Equivalently: find the last k such that lines[k].textStart <= cursorByte.
		kCursor = 0;
		for (std::size_t k = 0; k < lines.size(); ++k)
		{
			if (lines[k].textStart <= cursorByte)
			{
				kCursor = k;
			}
			else
			{
				break;
			}
		}

		// Count non-\r bytes in text from lines[kCursor].textStart up to (but not including)
		// cursorByte to get columnBytes.
		columnBytes = 0;
		const std::size_t lineRawStart = lines[kCursor].textStart;
		for (std::size_t i = lineRawStart; i < cursorByte && i < text.size(); ++i)
		{
			if (text[i] != '\r')
			{
				++columnBytes;
			}
		}
	}

	// Compute cursor x by summing advances of emitted bytes [0, columnBytes)
	const LineInfo& cursorLine = lines[kCursor];
	float cursorX = textX;
	for (std::size_t col = 0; col < columnBytes && col < cursorLine.bytes.size(); ++col)
	{
		const uint8_t b = static_cast<uint8_t>(cursorLine.bytes[col]);
		cursorX += atlas.GetGlyph(b).advance;
	}

	const float cursorTop = y + _config.paddingY + static_cast<float>(kCursor) * advance;
	const float cursorHeight = atlas.GetAscender() - atlas.GetDescender();

	if (cursorStyle == CursorStyle::Block)
	{
		float blockWidth;
		if (columnBytes < cursorLine.bytes.size())
		{
			const uint8_t b = static_cast<uint8_t>(cursorLine.bytes[columnBytes]);
			blockWidth = atlas.GetGlyph(b).advance;
		}
		else
		{
			blockWidth = atlas.GetGlyph(0x20u).advance;
		}
		out.AddQuad(QuadCommand{cursorX, cursorTop, blockWidth, cursorHeight, _config.cursor});
	}
	else // CursorStyle::Insertion
	{
		out.AddQuad(QuadCommand{cursorX, cursorTop, _config.cursorBarWidth, cursorHeight, _config.cursor});
	}
}

} // namespace ui
} // namespace virasa
