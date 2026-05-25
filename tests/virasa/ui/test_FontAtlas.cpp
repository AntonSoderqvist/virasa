#include <gtest/gtest.h>

#include "virasa/ui/FontAtlas.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace virasa::ui;

namespace
{

std::string FindReadableTestFontPath()
{
	static const char* kCandidates[] = {
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/TTF/DejaVuSans.ttf",
		"/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
		"/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
		"/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
		"/System/Library/Fonts/Supplemental/Arial.ttf",
		"/Library/Fonts/Arial.ttf",
		"C:/Windows/Fonts/arial.ttf",
		"C:/Windows/Fonts/segoeui.ttf"
	};

	for (const char* candidate : kCandidates)
	{
		std::error_code ec;
		if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec))
		{
			std::ifstream stream(candidate, std::ios::binary);
			if (stream.good())
			{
				return std::string(candidate);
			}
		}
	}

	return {};
}

const std::string& GetTestFontPath()
{
	static const std::string path = FindReadableTestFontPath();
	return path;
}

void RequireTestFont()
{
	ASSERT_FALSE(GetTestFontPath().empty());
}

} // namespace

TEST(FontAtlas, test_font_atlas_error_enum_values_in_declared_order)
{
	static_assert(std::is_enum_v<FontAtlasError>);
	static_assert(std::is_same_v<std::underlying_type_t<FontAtlasError>, uint32_t>);

	EXPECT_EQ(static_cast<uint32_t>(FontAtlasError::None), 0u);
	EXPECT_EQ(static_cast<uint32_t>(FontAtlasError::FileNotFound), 1u);
	EXPECT_EQ(static_cast<uint32_t>(FontAtlasError::FreeTypeInitFailed), 2u);
	EXPECT_EQ(static_cast<uint32_t>(FontAtlasError::FaceLoadFailed), 3u);
	EXPECT_EQ(static_cast<uint32_t>(FontAtlasError::AtlasTooSmall), 4u);

	FontAtlas atlas;
	const FontAtlasError result = atlas.Initialize("definitely_missing_font_file.ttf", 16u);
	EXPECT_EQ(result, FontAtlasError::FileNotFound);
	EXPECT_TRUE(atlas.GetBitmap().empty());
	EXPECT_EQ(atlas.GetAtlasWidth(), 0u);
	EXPECT_EQ(atlas.GetAtlasHeight(), 0u);
	EXPECT_EQ(atlas.GetPixelSize(), 0u);
	EXPECT_FLOAT_EQ(atlas.GetLineHeight(), 0.0f);
	EXPECT_FLOAT_EQ(atlas.GetAscender(), 0.0f);
	EXPECT_FLOAT_EQ(atlas.GetDescender(), 0.0f);
}

TEST(FontAtlas, test_glyph_metrics_is_plain_pod_with_pixel_uv_coordinates)
{
	static_assert(std::is_default_constructible_v<GlyphMetrics>);
	static_assert(std::is_copy_constructible_v<GlyphMetrics>);
	static_assert(std::is_copy_assignable_v<GlyphMetrics>);
	static_assert(std::is_move_constructible_v<GlyphMetrics>);
	static_assert(std::is_move_assignable_v<GlyphMetrics>);
	static_assert(std::is_trivially_destructible_v<GlyphMetrics>);

	EXPECT_EQ(offsetof(GlyphMetrics, advance), 0u);
	EXPECT_EQ(offsetof(GlyphMetrics, bearingX), sizeof(float));
	EXPECT_EQ(offsetof(GlyphMetrics, bearingY), sizeof(float) + sizeof(int16_t));
	EXPECT_EQ(offsetof(GlyphMetrics, width), sizeof(float) + sizeof(int16_t) * 2u);
	EXPECT_EQ(offsetof(GlyphMetrics, height), sizeof(float) + sizeof(int16_t) * 2u + sizeof(uint16_t));
	EXPECT_EQ(offsetof(GlyphMetrics, u0), sizeof(float) + sizeof(int16_t) * 2u + sizeof(uint16_t) * 2u);
	EXPECT_EQ(offsetof(GlyphMetrics, v0), sizeof(float) + sizeof(int16_t) * 2u + sizeof(uint16_t) * 3u);
	EXPECT_EQ(offsetof(GlyphMetrics, u1), sizeof(float) + sizeof(int16_t) * 2u + sizeof(uint16_t) * 4u);
	EXPECT_EQ(offsetof(GlyphMetrics, v1), sizeof(float) + sizeof(int16_t) * 2u + sizeof(uint16_t) * 5u);

	GlyphMetrics metrics;
	EXPECT_FLOAT_EQ(metrics.advance, 0.0f);
	EXPECT_EQ(metrics.bearingX, 0);
	EXPECT_EQ(metrics.bearingY, 0);
	EXPECT_EQ(metrics.width, 0u);
	EXPECT_EQ(metrics.height, 0u);
	EXPECT_EQ(metrics.u0, 0u);
	EXPECT_EQ(metrics.v0, 0u);
	EXPECT_EQ(metrics.u1, 0u);
	EXPECT_EQ(metrics.v1, 0u);
}

TEST(FontAtlas, test_font_atlas_default_constructed_is_empty_and_uninitialized)
{
	static_assert(std::is_default_constructible_v<FontAtlas>);
	static_assert(std::is_final_v<FontAtlas>);
	static_assert(!std::is_copy_constructible_v<FontAtlas>);
	static_assert(!std::is_copy_assignable_v<FontAtlas>);
	static_assert(std::is_move_constructible_v<FontAtlas>);
	static_assert(std::is_move_assignable_v<FontAtlas>);

	FontAtlas atlas;
	EXPECT_TRUE(atlas.GetBitmap().empty());
	EXPECT_EQ(atlas.GetAtlasWidth(), 0u);
	EXPECT_EQ(atlas.GetAtlasHeight(), 0u);
	EXPECT_EQ(atlas.GetPixelSize(), 0u);
	EXPECT_FLOAT_EQ(atlas.GetLineHeight(), 0.0f);
	EXPECT_FLOAT_EQ(atlas.GetAscender(), 0.0f);
	EXPECT_FLOAT_EQ(atlas.GetDescender(), 0.0f);
	EXPECT_FALSE(atlas.HasGlyph(0u));
	EXPECT_FALSE(atlas.HasGlyph(static_cast<uint32_t>('A')));
	EXPECT_FALSE(atlas.HasGlyph(0x20u));
	EXPECT_FALSE(atlas.HasGlyph(0x7Eu));
	EXPECT_FALSE(atlas.HasGlyph(0x80u));

	const GlyphMetrics& missing = atlas.GetGlyph(static_cast<uint32_t>('A'));
	EXPECT_FLOAT_EQ(missing.advance, 0.0f);
	EXPECT_EQ(missing.bearingX, 0);
	EXPECT_EQ(missing.bearingY, 0);
	EXPECT_EQ(missing.width, 0u);
	EXPECT_EQ(missing.height, 0u);
	EXPECT_EQ(missing.u0, 0u);
	EXPECT_EQ(missing.v0, 0u);
	EXPECT_EQ(missing.u1, 0u);
	EXPECT_EQ(missing.v1, 0u);

	FontAtlas movedTo = std::move(atlas);
	EXPECT_TRUE(atlas.GetBitmap().empty());
	EXPECT_EQ(atlas.GetAtlasWidth(), 0u);
	EXPECT_EQ(atlas.GetAtlasHeight(), 0u);
	EXPECT_EQ(atlas.GetPixelSize(), 0u);
	EXPECT_FLOAT_EQ(atlas.GetLineHeight(), 0.0f);
	EXPECT_FLOAT_EQ(atlas.GetAscender(), 0.0f);
	EXPECT_FLOAT_EQ(atlas.GetDescender(), 0.0f);
	EXPECT_FALSE(atlas.HasGlyph(static_cast<uint32_t>('A')));

	EXPECT_TRUE(movedTo.GetBitmap().empty());
	EXPECT_EQ(movedTo.GetAtlasWidth(), 0u);
	EXPECT_EQ(movedTo.GetAtlasHeight(), 0u);
	EXPECT_EQ(movedTo.GetPixelSize(), 0u);
	EXPECT_FLOAT_EQ(movedTo.GetLineHeight(), 0.0f);
	EXPECT_FLOAT_EQ(movedTo.GetAscender(), 0.0f);
	EXPECT_FLOAT_EQ(movedTo.GetDescender(), 0.0f);
	EXPECT_FALSE(movedTo.HasGlyph(static_cast<uint32_t>('A')));
}

TEST(FontAtlas, test_initialize_bakes_ascii_atlas_via_freetype)
{
	RequireTestFont();

	constexpr uint32_t kPixelSize = 32u;
	FontAtlas atlas;
	const FontAtlasError result = atlas.Initialize(GetTestFontPath(), kPixelSize);
	ASSERT_EQ(result, FontAtlasError::None);

	const std::span<const uint8_t> bitmap = atlas.GetBitmap();
	EXPECT_FALSE(bitmap.empty());
	EXPECT_GT(atlas.GetAtlasWidth(), 0u);
	EXPECT_GT(atlas.GetAtlasHeight(), 0u);
	EXPECT_EQ(atlas.GetPixelSize(), kPixelSize);
	EXPECT_GT(atlas.GetLineHeight(), 0.0f);
	EXPECT_GT(atlas.GetAscender(), 0.0f);
	EXPECT_LT(atlas.GetDescender(), 0.0f);

	for (uint32_t codepoint = 0x20u; codepoint <= 0x7Eu; ++codepoint)
	{
		EXPECT_TRUE(atlas.HasGlyph(codepoint));
	}
}

TEST(FontAtlas, test_atlas_dimensions_capped_at_512x512_shelf_packed)
{
	RequireTestFont();

	FontAtlas atlas;
	const FontAtlasError result = atlas.Initialize(GetTestFontPath(), 32u);
	ASSERT_EQ(result, FontAtlasError::None);

	EXPECT_GT(atlas.GetAtlasWidth(), 0u);
	EXPECT_GT(atlas.GetAtlasHeight(), 0u);
	EXPECT_LE(atlas.GetAtlasWidth(), 512u);
	EXPECT_LE(atlas.GetAtlasHeight(), 512u);

	for (uint32_t codepoint = 0x20u; codepoint <= 0x7Eu; ++codepoint)
	{
		const GlyphMetrics& glyph = atlas.GetGlyph(codepoint);
		EXPECT_LE(glyph.u0, atlas.GetAtlasWidth());
		EXPECT_LE(glyph.u1, atlas.GetAtlasWidth());
		EXPECT_LE(glyph.v0, atlas.GetAtlasHeight());
		EXPECT_LE(glyph.v1, atlas.GetAtlasHeight());

		if (glyph.width == 0u || glyph.height == 0u)
		{
			EXPECT_EQ(glyph.width, 0u);
			EXPECT_EQ(glyph.height, 0u);
			EXPECT_EQ(glyph.u0, 0u);
			EXPECT_EQ(glyph.v0, 0u);
			EXPECT_EQ(glyph.u1, 0u);
			EXPECT_EQ(glyph.v1, 0u);
		}
		else
		{
			EXPECT_EQ(glyph.u1, static_cast<uint16_t>(glyph.u0 + glyph.width));
			EXPECT_EQ(glyph.v1, static_cast<uint16_t>(glyph.v0 + glyph.height));
		}
	}
}

TEST(FontAtlas, test_get_bitmap_returns_r8_row_major_span_with_atlas_lifetime)
{
	RequireTestFont();

	FontAtlas atlas;
	ASSERT_EQ(atlas.Initialize(GetTestFontPath(), 24u), FontAtlasError::None);

	const std::span<const uint8_t> bitmap = atlas.GetBitmap();
	ASSERT_FALSE(bitmap.empty());
	EXPECT_EQ(bitmap.size(),
		static_cast<std::size_t>(atlas.GetAtlasWidth()) * static_cast<std::size_t>(atlas.GetAtlasHeight()));

	const std::span<const uint8_t> bitmapAgain = atlas.GetBitmap();
	EXPECT_EQ(bitmap.data(), bitmapAgain.data());
	EXPECT_EQ(bitmap.size(), bitmapAgain.size());

	bool foundCoveredPixel = false;
	for (uint8_t value : bitmap)
	{
		if (value > 0u)
		{
			foundCoveredPixel = true;
			break;
		}
	}
	EXPECT_TRUE(foundCoveredPixel);

	FontAtlas movedTo = std::move(atlas);
	const std::span<const uint8_t> movedBitmap = movedTo.GetBitmap();
	EXPECT_FALSE(movedBitmap.empty());
	EXPECT_EQ(movedBitmap.size(), bitmap.size());
	EXPECT_EQ(atlas.GetBitmap().size(), 0u);
}

TEST(FontAtlas, test_get_glyph_returns_metrics_or_missing_fallback)
{
	static_assert(noexcept(std::declval<const FontAtlas&>().GetGlyph(0u)));
	static_assert(noexcept(std::declval<const FontAtlas&>().HasGlyph(0u)));

	FontAtlas atlas;
	const GlyphMetrics& missingBeforeInit = atlas.GetGlyph(static_cast<uint32_t>('A'));
	EXPECT_FALSE(atlas.HasGlyph(static_cast<uint32_t>('A')));
	EXPECT_FLOAT_EQ(missingBeforeInit.advance, 0.0f);
	EXPECT_EQ(missingBeforeInit.bearingX, 0);
	EXPECT_EQ(missingBeforeInit.bearingY, 0);
	EXPECT_EQ(missingBeforeInit.width, 0u);
	EXPECT_EQ(missingBeforeInit.height, 0u);
	EXPECT_EQ(missingBeforeInit.u0, 0u);
	EXPECT_EQ(missingBeforeInit.v0, 0u);
	EXPECT_EQ(missingBeforeInit.u1, 0u);
	EXPECT_EQ(missingBeforeInit.v1, 0u);

	RequireTestFont();
	ASSERT_EQ(atlas.Initialize(GetTestFontPath(), 24u), FontAtlasError::None);

	ASSERT_TRUE(atlas.HasGlyph(static_cast<uint32_t>('A')));
	const GlyphMetrics& glyphA = atlas.GetGlyph(static_cast<uint32_t>('A'));
	const GlyphMetrics& glyphAAgain = atlas.GetGlyph(static_cast<uint32_t>('A'));
	EXPECT_EQ(&glyphA, &glyphAAgain);
	EXPECT_GT(glyphA.advance, 0.0f);

	EXPECT_FALSE(atlas.HasGlyph(0u));
	const GlyphMetrics& missing = atlas.GetGlyph(0u);
	EXPECT_FLOAT_EQ(missing.advance, 0.0f);
	EXPECT_EQ(missing.bearingX, 0);
	EXPECT_EQ(missing.bearingY, 0);
	EXPECT_EQ(missing.width, 0u);
	EXPECT_EQ(missing.height, 0u);
	EXPECT_EQ(missing.u0, 0u);
	EXPECT_EQ(missing.v0, 0u);
	EXPECT_EQ(missing.u1, 0u);
	EXPECT_EQ(missing.v1, 0u);
}

TEST(FontAtlas, test_face_metrics_observers_return_pixel_values)
{
	static_assert(noexcept(std::declval<const FontAtlas&>().GetPixelSize()));
	static_assert(noexcept(std::declval<const FontAtlas&>().GetLineHeight()));
	static_assert(noexcept(std::declval<const FontAtlas&>().GetAscender()));
	static_assert(noexcept(std::declval<const FontAtlas&>().GetDescender()));

	RequireTestFont();

	constexpr uint32_t kPixelSize = 28u;
	FontAtlas atlas;
	ASSERT_EQ(atlas.Initialize(GetTestFontPath(), kPixelSize), FontAtlasError::None);

	FT_Library library = nullptr;
	ASSERT_EQ(FT_Init_FreeType(&library), 0);

	FT_Face face = nullptr;
	ASSERT_EQ(FT_New_Face(library, GetTestFontPath().c_str(), 0, &face), 0);
	ASSERT_EQ(FT_Set_Pixel_Sizes(face, 0, kPixelSize), 0);

	const float expectedLineHeight = static_cast<float>(face->size->metrics.height) / 64.0f;
	const float expectedAscender = static_cast<float>(face->size->metrics.ascender) / 64.0f;
	const float expectedDescender = static_cast<float>(face->size->metrics.descender) / 64.0f;

	EXPECT_EQ(atlas.GetPixelSize(), kPixelSize);
	EXPECT_FLOAT_EQ(atlas.GetLineHeight(), expectedLineHeight);
	EXPECT_FLOAT_EQ(atlas.GetAscender(), expectedAscender);
	EXPECT_FLOAT_EQ(atlas.GetDescender(), expectedDescender);

	EXPECT_GT(atlas.GetLineHeight(), 0.0f);
	EXPECT_GT(atlas.GetAscender(), 0.0f);
	EXPECT_LT(atlas.GetDescender(), 0.0f);

	FT_Done_Face(face);
	FT_Done_FreeType(library);
}
