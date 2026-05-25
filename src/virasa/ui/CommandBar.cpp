#include "virasa/ui/CommandBar.h"

#include <algorithm>

namespace virasa
{
namespace ui
{
namespace
{
struct Utf8DecodeResult
{
	uint32_t codepoint = 0u;
	std::size_t length = 0u;
};

[[nodiscard]] Utf8DecodeResult DecodeUtf8Codepoint(
	std::string_view text, std::size_t offset) noexcept
{
	if (offset >= text.size())
	{
		return {};
	}

	const auto byte0 = static_cast<unsigned char>(text[offset]);
	if (byte0 < 0x80u)
	{
		return {static_cast<uint32_t>(byte0), 1u};
	}

	if ((byte0 & 0xE0u) == 0xC0u)
	{
		if (offset + 1u < text.size())
		{
			const auto byte1 = static_cast<unsigned char>(text[offset + 1u]);
			if ((byte1 & 0xC0u) == 0x80u)
			{
				const uint32_t codepoint = (static_cast<uint32_t>(byte0 & 0x1Fu) << 6u) |
								   static_cast<uint32_t>(byte1 & 0x3Fu);
				if (codepoint >= 0x80u)
				{
					return {codepoint, 2u};
				}
			}
		}

		return {0xFFFFFFFFu, 1u};
	}

	if ((byte0 & 0xF0u) == 0xE0u)
	{
		if (offset + 2u < text.size())
		{
			const auto byte1 = static_cast<unsigned char>(text[offset + 1u]);
			const auto byte2 = static_cast<unsigned char>(text[offset + 2u]);
			if (((byte1 & 0xC0u) == 0x80u) && ((byte2 & 0xC0u) == 0x80u))
			{
				const uint32_t codepoint = (static_cast<uint32_t>(byte0 & 0x0Fu) << 12u) |
								   (static_cast<uint32_t>(byte1 & 0x3Fu) << 6u) |
								   static_cast<uint32_t>(byte2 & 0x3Fu);
				if ((codepoint >= 0x800u) &&
					!(codepoint >= 0xD800u && codepoint <= 0xDFFFu))
				{
					return {codepoint, 3u};
				}
			}
		}

		return {0xFFFFFFFFu, 1u};
	}

	if ((byte0 & 0xF8u) == 0xF0u)
	{
		if (offset + 3u < text.size())
		{
			const auto byte1 = static_cast<unsigned char>(text[offset + 1u]);
			const auto byte2 = static_cast<unsigned char>(text[offset + 2u]);
			const auto byte3 = static_cast<unsigned char>(text[offset + 3u]);
			if (((byte1 & 0xC0u) == 0x80u) && ((byte2 & 0xC0u) == 0x80u) &&
				((byte3 & 0xC0u) == 0x80u))
			{
				const uint32_t codepoint = (static_cast<uint32_t>(byte0 & 0x07u) << 18u) |
								   (static_cast<uint32_t>(byte1 & 0x3Fu) << 12u) |
								   (static_cast<uint32_t>(byte2 & 0x3Fu) << 6u) |
								   static_cast<uint32_t>(byte3 & 0x3Fu);
				if (codepoint >= 0x10000u && codepoint <= 0x10FFFFu)
				{
					return {codepoint, 4u};
				}
			}
		}

		return {0xFFFFFFFFu, 1u};
	}

	return {0xFFFFFFFFu, 1u};
}
} // namespace

void CommandBar::SetConfig(const CommandBarConfig& config) noexcept
{
	_config = config;
}

const CommandBarConfig& CommandBar::GetConfig() const noexcept
{
	return _config;
}

void CommandBar::Render(DrawList& out, std::string_view text, std::size_t cursorByteIndex,
	const FontAtlas& atlas, uint32_t windowWidth, uint32_t windowHeight) const
{
	const float lineHeight = atlas.GetAscender() - atlas.GetDescender();
	const float barHeight = lineHeight + 2.0f * _config.paddingY;
	const float barTop = static_cast<float>(windowHeight) - barHeight;
	const float baselineY = barTop + _config.paddingY + atlas.GetAscender();

	out.AddQuad(
		{0.0f, barTop, static_cast<float>(windowWidth), barHeight, _config.background});

	out.AddText(_config.paddingX, baselineY, text, _config.foreground);

	const std::size_t clampedCursorByteIndex = std::min(cursorByteIndex, text.size());
	float cursorX = 0.0f;
	std::size_t offset = 0u;
	while (offset < clampedCursorByteIndex)
	{
		const Utf8DecodeResult decoded = DecodeUtf8Codepoint(text, offset);
		if (decoded.length == 0u)
		{
			break;
		}

		cursorX += atlas.GetGlyph(decoded.codepoint).advance;
		offset += decoded.length;
	}

	float cursorWidth = 0.0f;
	if (clampedCursorByteIndex == text.size())
	{
		cursorWidth = atlas.GetGlyph(0x20u).advance;
	}
	else
	{
		const Utf8DecodeResult next = DecodeUtf8Codepoint(text, clampedCursorByteIndex);
		if (next.length != 0u)
		{
			cursorWidth = atlas.GetGlyph(next.codepoint).advance;
		}

		if (cursorWidth == 0.0f)
		{
			cursorWidth = atlas.GetGlyph(0x20u).advance;
		}
	}

	if (cursorWidth == 0.0f)
	{
		cursorWidth = 0.6f * lineHeight;
	}

	out.AddQuad({_config.paddingX + cursorX,
		barTop + _config.paddingY,
		cursorWidth,
		lineHeight,
		_config.cursorColor});
}

} // namespace ui
} // namespace virasa
