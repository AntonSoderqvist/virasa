#include "virasa/ui/FontAtlas.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>

namespace virasa
{
namespace ui
{
namespace
{
constexpr uint32_t kFirstAscii = 0x20u;
constexpr uint32_t kLastAscii = 0x7Eu;
constexpr uint32_t kGlyphCount = kLastAscii - kFirstAscii + 1u;
constexpr uint32_t kMaxAtlasWidth = 512u;
constexpr uint32_t kMaxAtlasHeight = 512u;

struct BakedGlyph
{
	GlyphMetrics metrics = {};
	std::vector<uint8_t> bitmap = {};
};

void ResetToDefault(FontAtlas& atlas) noexcept
{
	atlas = FontAtlas{};
}
} // namespace

FontAtlas::FontAtlas(FontAtlas&& other) noexcept
    : _bitmap(std::move(other._bitmap)), _glyphs(std::move(other._glyphs)),
	_missing(other._missing), _atlasWidth(other._atlasWidth), _atlasHeight(other._atlasHeight),
	_pixelSize(other._pixelSize), _lineHeight(other._lineHeight), _ascender(other._ascender),
	_descender(other._descender), _initialized(other._initialized)
{
	other._missing = {};
	other._atlasWidth = 0u;
	other._atlasHeight = 0u;
	other._pixelSize = 0u;
	other._lineHeight = 0.0f;
	other._ascender = 0.0f;
	other._descender = 0.0f;
	other._initialized = false;
}

FontAtlas& FontAtlas::operator=(FontAtlas&& other) noexcept
{
	if (this != &other)
	{
		_bitmap = std::move(other._bitmap);
		_glyphs = std::move(other._glyphs);
		_missing = other._missing;
		_atlasWidth = other._atlasWidth;
		_atlasHeight = other._atlasHeight;
		_pixelSize = other._pixelSize;
		_lineHeight = other._lineHeight;
		_ascender = other._ascender;
		_descender = other._descender;
		_initialized = other._initialized;

		other._missing = {};
		other._atlasWidth = 0u;
		other._atlasHeight = 0u;
		other._pixelSize = 0u;
		other._lineHeight = 0.0f;
		other._ascender = 0.0f;
		other._descender = 0.0f;
		other._initialized = false;
	}

	return *this;
}

FontAtlasError FontAtlas::Initialize(std::string_view ttfPath, uint32_t pixelSize)
{
	std::ifstream file(std::string(ttfPath), std::ios::binary);
	if (!file.good())
	{
		ResetToDefault(*this);
		return FontAtlasError::FileNotFound;
	}

	FT_Library library = nullptr;
	if (FT_Init_FreeType(&library) != 0)
	{
		ResetToDefault(*this);
		return FontAtlasError::FreeTypeInitFailed;
	}

	FT_Face face = nullptr;
	const std::string path(ttfPath);
	if (FT_New_Face(library, path.c_str(), 0, &face) != 0)
	{
		FT_Done_FreeType(library);
		ResetToDefault(*this);
		return FontAtlasError::FaceLoadFailed;
	}

	if (FT_Set_Pixel_Sizes(face, 0, pixelSize) != 0)
	{
		FT_Done_Face(face);
		FT_Done_FreeType(library);
		ResetToDefault(*this);
		return FontAtlasError::FaceLoadFailed;
	}

	std::array<BakedGlyph, kGlyphCount> bakedGlyphs = {};
	uint32_t penX = 0u;
	uint32_t penY = 0u;
	uint32_t rowHeight = 0u;
	uint32_t usedWidth = 0u;
	uint32_t usedHeight = 0u;

	for (uint32_t codepoint = kFirstAscii; codepoint <= kLastAscii; ++codepoint)
	{
		const uint32_t glyphIndex = codepoint - kFirstAscii;
		if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER) != 0)
		{
			FT_Done_Face(face);
			FT_Done_FreeType(library);
			ResetToDefault(*this);
			return FontAtlasError::FaceLoadFailed;
		}

		const FT_GlyphSlot glyph = face->glyph;
		const uint32_t glyphWidth = static_cast<uint32_t>(glyph->bitmap.width);
		const uint32_t glyphHeight = static_cast<uint32_t>(glyph->bitmap.rows);

		BakedGlyph baked = {};
		baked.metrics.advance = static_cast<float>(glyph->advance.x) / 64.0f;
		baked.metrics.bearingX = static_cast<int16_t>(glyph->bitmap_left);
		baked.metrics.bearingY = static_cast<int16_t>(glyph->bitmap_top);
		baked.metrics.width = static_cast<uint16_t>(glyphWidth);
		baked.metrics.height = static_cast<uint16_t>(glyphHeight);

		if (glyphWidth > 0u && glyphHeight > 0u)
		{
			if (penX + glyphWidth > kMaxAtlasWidth)
			{
				penX = 0u;
				penY += rowHeight;
				rowHeight = 0u;
			}

			if (penY + glyphHeight > kMaxAtlasHeight)
			{
				FT_Done_Face(face);
				FT_Done_FreeType(library);
				ResetToDefault(*this);
				return FontAtlasError::AtlasTooSmall;
			}

			baked.metrics.u0 = static_cast<uint16_t>(penX);
			baked.metrics.v0 = static_cast<uint16_t>(penY);
			baked.metrics.u1 = static_cast<uint16_t>(penX + glyphWidth);
			baked.metrics.v1 = static_cast<uint16_t>(penY + glyphHeight);

			baked.bitmap.resize(glyphWidth * glyphHeight);
			for (uint32_t row = 0u; row < glyphHeight; ++row)
			{
				const uint8_t* sourceRow =
					glyph->bitmap.buffer +
					static_cast<std::size_t>(row) *
						static_cast<std::size_t>(glyph->bitmap.pitch);
				uint8_t* destinationRow =
					baked.bitmap.data() +
					static_cast<std::size_t>(row) * static_cast<std::size_t>(glyphWidth);
				std::memcpy(destinationRow, sourceRow, glyphWidth);
			}

			penX += glyphWidth;
			rowHeight = std::max(rowHeight, glyphHeight);
			usedWidth = std::max(usedWidth, penX);
			usedHeight = std::max(usedHeight, penY + glyphHeight);
		}

		bakedGlyphs[glyphIndex] = std::move(baked);
	}

	const float lineHeightPx = static_cast<float>(face->size->metrics.height) / 64.0f;
	const float ascenderPx = static_cast<float>(face->size->metrics.ascender) / 64.0f;
	const float descenderPx = static_cast<float>(face->size->metrics.descender) / 64.0f;

	FT_Done_Face(face);
	FT_Done_FreeType(library);

	FontAtlas newAtlas;
	newAtlas._atlasWidth = usedWidth;
	newAtlas._atlasHeight = usedHeight;
	newAtlas._pixelSize = pixelSize;
	newAtlas._lineHeight = lineHeightPx;
	newAtlas._ascender = ascenderPx;
	newAtlas._descender = descenderPx;
	newAtlas._glyphs.resize(kGlyphCount);

	if (newAtlas._atlasWidth > 0u && newAtlas._atlasHeight > 0u)
	{
		newAtlas._bitmap.resize(static_cast<std::size_t>(newAtlas._atlasWidth) *
							static_cast<std::size_t>(newAtlas._atlasHeight),
			0u);
	}

	for (uint32_t index = 0u; index < kGlyphCount; ++index)
	{
		newAtlas._glyphs[index] = bakedGlyphs[index].metrics;
		const GlyphMetrics& metrics = bakedGlyphs[index].metrics;
		if (metrics.width == 0u || metrics.height == 0u)
		{
			continue;
		}

		for (uint32_t row = 0u; row < metrics.height; ++row)
		{
			const std::size_t atlasOffset =
				static_cast<std::size_t>(metrics.v0 + row) *
					static_cast<std::size_t>(newAtlas._atlasWidth) +
				static_cast<std::size_t>(metrics.u0);
			const std::size_t glyphOffset =
				static_cast<std::size_t>(row) * static_cast<std::size_t>(metrics.width);
			std::memcpy(newAtlas._bitmap.data() + atlasOffset,
				bakedGlyphs[index].bitmap.data() + glyphOffset,
				metrics.width);
		}
	}

	newAtlas._initialized = true;
	*this = std::move(newAtlas);
	return FontAtlasError::None;
}

const GlyphMetrics& FontAtlas::GetGlyph(uint32_t codepoint) const noexcept
{
	if (HasGlyph(codepoint))
	{
		return _glyphs[codepoint - kFirstAscii];
	}

	return _missing;
}

bool FontAtlas::HasGlyph(uint32_t codepoint) const noexcept
{
	return _initialized && codepoint >= kFirstAscii && codepoint <= kLastAscii &&
		 _glyphs.size() == kGlyphCount;
}

std::span<const uint8_t> FontAtlas::GetBitmap() const noexcept
{
	return std::span<const uint8_t>(_bitmap.data(), _bitmap.size());
}

uint32_t FontAtlas::GetAtlasWidth() const noexcept
{
	return _atlasWidth;
}

uint32_t FontAtlas::GetAtlasHeight() const noexcept
{
	return _atlasHeight;
}

uint32_t FontAtlas::GetPixelSize() const noexcept
{
	return _pixelSize;
}

float FontAtlas::GetLineHeight() const noexcept
{
	return _lineHeight;
}

float FontAtlas::GetAscender() const noexcept
{
	return _ascender;
}

float FontAtlas::GetDescender() const noexcept
{
	return _descender;
}
} // namespace ui
} // namespace virasa
