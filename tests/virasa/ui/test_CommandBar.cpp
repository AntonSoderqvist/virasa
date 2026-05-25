#include <gtest/gtest.h>

#include "virasa/ui/CommandBar.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/Types.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

using namespace virasa::ui;

namespace
{

constexpr float kFloatEpsilon = 1e-5f;

void ExpectColorEq(const Color& actual, const Color& expected)
{
	EXPECT_FLOAT_EQ(actual.r, expected.r);
	EXPECT_FLOAT_EQ(actual.g, expected.g);
	EXPECT_FLOAT_EQ(actual.b, expected.b);
	EXPECT_FLOAT_EQ(actual.a, expected.a);
}

float ComputeCursorAdvance(std::string_view text, std::size_t cursorByteIndex, const FontAtlas& atlas)
{
	const std::size_t clampedIndex = (cursorByteIndex < text.size()) ? cursorByteIndex : text.size();
	float cursorX = 0.0f;
	std::size_t i = 0;

	while (i < clampedIndex)
	{
		const unsigned char lead = static_cast<unsigned char>(text[i]);
		uint32_t codepoint = 0u;
		std::size_t advanceBytes = 1;
		bool valid = false;

		if (lead < 0x80u)
		{
			codepoint = lead;
			advanceBytes = 1;
			valid = true;
		}
		else if ((lead & 0xE0u) == 0xC0u && i + 1 < clampedIndex)
		{
			const unsigned char b1 = static_cast<unsigned char>(text[i + 1]);
			if ((b1 & 0xC0u) == 0x80u)
			{
				codepoint = static_cast<uint32_t>(lead & 0x1Fu) << 6;
				codepoint |= static_cast<uint32_t>(b1 & 0x3Fu);
				advanceBytes = 2;
				valid = true;
			}
		}
		else if ((lead & 0xF0u) == 0xE0u && i + 2 < clampedIndex)
		{
			const unsigned char b1 = static_cast<unsigned char>(text[i + 1]);
			const unsigned char b2 = static_cast<unsigned char>(text[i + 2]);
			if (((b1 & 0xC0u) == 0x80u) && ((b2 & 0xC0u) == 0x80u))
			{
				codepoint = static_cast<uint32_t>(lead & 0x0Fu) << 12;
				codepoint |= static_cast<uint32_t>(b1 & 0x3Fu) << 6;
				codepoint |= static_cast<uint32_t>(b2 & 0x3Fu);
				advanceBytes = 3;
				valid = true;
			}
		}
		else if ((lead & 0xF8u) == 0xF0u && i + 3 < clampedIndex)
		{
			const unsigned char b1 = static_cast<unsigned char>(text[i + 1]);
			const unsigned char b2 = static_cast<unsigned char>(text[i + 2]);
			const unsigned char b3 = static_cast<unsigned char>(text[i + 3]);
			if (((b1 & 0xC0u) == 0x80u) && ((b2 & 0xC0u) == 0x80u) && ((b3 & 0xC0u) == 0x80u))
			{
				codepoint = static_cast<uint32_t>(lead & 0x07u) << 18;
				codepoint |= static_cast<uint32_t>(b1 & 0x3Fu) << 12;
				codepoint |= static_cast<uint32_t>(b2 & 0x3Fu) << 6;
				codepoint |= static_cast<uint32_t>(b3 & 0x3Fu);
				advanceBytes = 4;
				valid = true;
			}
		}

		if (valid)
		{
			cursorX += atlas.GetGlyph(codepoint).advance;
			i += advanceBytes;
		}
		else
		{
			cursorX += atlas.GetGlyph(0xFFFFFFFFu).advance;
			++i;
		}
	}

	return cursorX;
}

float ComputeExpectedCursorWidth(
	std::string_view text,
	std::size_t cursorByteIndex,
	const FontAtlas& atlas)
{
	const std::size_t clampedIndex = (cursorByteIndex < text.size()) ? cursorByteIndex : text.size();
	const float lineHeight = atlas.GetAscender() - atlas.GetDescender();
	const float spaceAdvance = atlas.GetGlyph(0x20u).advance;
	const float fallbackWidth = (spaceAdvance == 0.0f) ? (0.6f * lineHeight) : spaceAdvance;

	if (clampedIndex == text.size())
	{
		return fallbackWidth;
	}

	const unsigned char lead = static_cast<unsigned char>(text[clampedIndex]);
	uint32_t codepoint = 0u;
	bool valid = false;

	if (lead < 0x80u)
	{
		codepoint = lead;
		valid = true;
	}
	else if ((lead & 0xE0u) == 0xC0u && clampedIndex + 1 < text.size())
	{
		const unsigned char b1 = static_cast<unsigned char>(text[clampedIndex + 1]);
		if ((b1 & 0xC0u) == 0x80u)
		{
			codepoint = static_cast<uint32_t>(lead & 0x1Fu) << 6;
			codepoint |= static_cast<uint32_t>(b1 & 0x3Fu);
			valid = true;
		}
	}
	else if ((lead & 0xF0u) == 0xE0u && clampedIndex + 2 < text.size())
	{
		const unsigned char b1 = static_cast<unsigned char>(text[clampedIndex + 1]);
		const unsigned char b2 = static_cast<unsigned char>(text[clampedIndex + 2]);
		if (((b1 & 0xC0u) == 0x80u) && ((b2 & 0xC0u) == 0x80u))
		{
			codepoint = static_cast<uint32_t>(lead & 0x0Fu) << 12;
			codepoint |= static_cast<uint32_t>(b1 & 0x3Fu) << 6;
			codepoint |= static_cast<uint32_t>(b2 & 0x3Fu);
			valid = true;
		}
	}
	else if ((lead & 0xF8u) == 0xF0u && clampedIndex + 3 < text.size())
	{
		const unsigned char b1 = static_cast<unsigned char>(text[clampedIndex + 1]);
		const unsigned char b2 = static_cast<unsigned char>(text[clampedIndex + 2]);
		const unsigned char b3 = static_cast<unsigned char>(text[clampedIndex + 3]);
		if (((b1 & 0xC0u) == 0x80u) && ((b2 & 0xC0u) == 0x80u) && ((b3 & 0xC0u) == 0x80u))
		{
			codepoint = static_cast<uint32_t>(lead & 0x07u) << 18;
			codepoint |= static_cast<uint32_t>(b1 & 0x3Fu) << 12;
			codepoint |= static_cast<uint32_t>(b2 & 0x3Fu) << 6;
			codepoint |= static_cast<uint32_t>(b3 & 0x3Fu);
			valid = true;
		}
	}

	const float width = valid ? atlas.GetGlyph(codepoint).advance : atlas.GetGlyph(0xFFFFFFFFu).advance;
	return (width == 0.0f) ? fallbackWidth : width;
}

} // namespace

TEST(CommandBar, test_command_bar_config_holds_default_overlay_appearance)
{
	static_assert(std::is_default_constructible_v<CommandBarConfig>);
	static_assert(std::is_copy_constructible_v<CommandBarConfig>);
	static_assert(std::is_move_constructible_v<CommandBarConfig>);
	static_assert(std::is_trivially_destructible_v<CommandBarConfig>);
	static_assert(std::is_standard_layout_v<CommandBarConfig>);

	const CommandBarConfig config;

	ExpectColorEq(config.background, Color{0.039f, 0.165f, 0.200f, 0.7f});
	ExpectColorEq(config.foreground, Color{0.9f, 0.9f, 0.9f, 1.0f});
	ExpectColorEq(config.cursorColor, Color{0.9f, 0.9f, 0.9f, 1.0f});
	EXPECT_FLOAT_EQ(config.paddingX, 6.0f);
	EXPECT_FLOAT_EQ(config.paddingY, 4.0f);
}

TEST(CommandBar, test_command_bar_is_stateless_with_respect_to_text_buffer)
{
	static_assert(std::is_default_constructible_v<CommandBar>);
	static_assert(std::is_copy_constructible_v<CommandBar>);
	static_assert(std::is_move_constructible_v<CommandBar>);
	static_assert(std::is_copy_assignable_v<CommandBar>);
	static_assert(std::is_move_assignable_v<CommandBar>);
	static_assert(std::is_final_v<CommandBar>);
	static_assert(noexcept(std::declval<CommandBar&>().SetConfig(std::declval<const CommandBarConfig&>())));
	static_assert(noexcept(std::declval<const CommandBar&>().GetConfig()));
	static_assert(std::is_same_v<
		decltype(std::declval<const CommandBar&>().GetConfig()),
		const CommandBarConfig&>);

	CommandBar bar;
	const CommandBarConfig defaultConfig;
	ExpectColorEq(bar.GetConfig().background, defaultConfig.background);
	ExpectColorEq(bar.GetConfig().foreground, defaultConfig.foreground);
	ExpectColorEq(bar.GetConfig().cursorColor, defaultConfig.cursorColor);
	EXPECT_FLOAT_EQ(bar.GetConfig().paddingX, defaultConfig.paddingX);
	EXPECT_FLOAT_EQ(bar.GetConfig().paddingY, defaultConfig.paddingY);

	CommandBarConfig customConfig;
	customConfig.background = {0.1f, 0.2f, 0.3f, 0.4f};
	customConfig.foreground = {0.5f, 0.6f, 0.7f, 0.8f};
	customConfig.cursorColor = {0.9f, 0.1f, 0.2f, 0.3f};
	customConfig.paddingX = 11.0f;
	customConfig.paddingY = 6.0f;

	bar.SetConfig(customConfig);
	customConfig.background = {9.0f, 9.0f, 9.0f, 9.0f};
	customConfig.foreground = {8.0f, 8.0f, 8.0f, 8.0f};
	customConfig.cursorColor = {7.0f, 7.0f, 7.0f, 7.0f};
	customConfig.paddingX = 77.0f;
	customConfig.paddingY = 77.0f;

	const CommandBarConfig& storedConfig = bar.GetConfig();
	ExpectColorEq(storedConfig.background, Color{0.1f, 0.2f, 0.3f, 0.4f});
	ExpectColorEq(storedConfig.foreground, Color{0.5f, 0.6f, 0.7f, 0.8f});
	ExpectColorEq(storedConfig.cursorColor, Color{0.9f, 0.1f, 0.2f, 0.3f});
	EXPECT_FLOAT_EQ(storedConfig.paddingX, 11.0f);
	EXPECT_FLOAT_EQ(storedConfig.paddingY, 6.0f);

	CommandBar copied = bar;
	ExpectColorEq(copied.GetConfig().background, storedConfig.background);
	ExpectColorEq(copied.GetConfig().foreground, storedConfig.foreground);
	ExpectColorEq(copied.GetConfig().cursorColor, storedConfig.cursorColor);
	EXPECT_FLOAT_EQ(copied.GetConfig().paddingX, storedConfig.paddingX);
	EXPECT_FLOAT_EQ(copied.GetConfig().paddingY, storedConfig.paddingY);

	CommandBar moved = std::move(copied);
	ExpectColorEq(moved.GetConfig().background, storedConfig.background);
	ExpectColorEq(moved.GetConfig().foreground, storedConfig.foreground);
	ExpectColorEq(moved.GetConfig().cursorColor, storedConfig.cursorColor);
	EXPECT_FLOAT_EQ(moved.GetConfig().paddingX, storedConfig.paddingX);
	EXPECT_FLOAT_EQ(moved.GetConfig().paddingY, storedConfig.paddingY);

	DrawList first;
	DrawList second;
	FontAtlas atlas;
	const std::string_view text = "state check";
	const std::size_t cursorByteIndex = 5;
	const uint32_t windowWidth = 320u;
	const uint32_t windowHeight = 120u;

	bar.Render(first, text, cursorByteIndex, atlas, windowWidth, windowHeight);
	bar.Render(second, text, cursorByteIndex, atlas, windowWidth, windowHeight);

	ASSERT_EQ(first.GetQuads().size(), second.GetQuads().size());
	ASSERT_EQ(first.GetTexts().size(), second.GetTexts().size());
	ASSERT_EQ(first.GetTextBuffer().size(), second.GetTextBuffer().size());
	EXPECT_EQ(first.GetTextBuffer(), second.GetTextBuffer());

	for (std::size_t i = 0; i < first.GetQuads().size(); ++i)
	{
		EXPECT_FLOAT_EQ(first.GetQuads()[i].x, second.GetQuads()[i].x);
		EXPECT_FLOAT_EQ(first.GetQuads()[i].y, second.GetQuads()[i].y);
		EXPECT_FLOAT_EQ(first.GetQuads()[i].width, second.GetQuads()[i].width);
		EXPECT_FLOAT_EQ(first.GetQuads()[i].height, second.GetQuads()[i].height);
		ExpectColorEq(first.GetQuads()[i].color, second.GetQuads()[i].color);
	}

	for (std::size_t i = 0; i < first.GetTexts().size(); ++i)
	{
		EXPECT_FLOAT_EQ(first.GetTexts()[i].x, second.GetTexts()[i].x);
		EXPECT_FLOAT_EQ(first.GetTexts()[i].y, second.GetTexts()[i].y);
		EXPECT_EQ(first.GetTexts()[i].textOffset, second.GetTexts()[i].textOffset);
		EXPECT_EQ(first.GetTexts()[i].textLength, second.GetTexts()[i].textLength);
		ExpectColorEq(first.GetTexts()[i].color, second.GetTexts()[i].color);
	}

	ExpectColorEq(bar.GetConfig().background, storedConfig.background);
	ExpectColorEq(bar.GetConfig().foreground, storedConfig.foreground);
	ExpectColorEq(bar.GetConfig().cursorColor, storedConfig.cursorColor);
	EXPECT_FLOAT_EQ(bar.GetConfig().paddingX, storedConfig.paddingX);
	EXPECT_FLOAT_EQ(bar.GetConfig().paddingY, storedConfig.paddingY);
}

TEST(CommandBar, test_render_appends_background_quad_at_bottom_of_window)
{
	CommandBar bar;
	CommandBarConfig config;
	config.paddingY = 8.0f;
	config.background = {0.2f, 0.3f, 0.4f, 0.5f};
	bar.SetConfig(config);

	DrawList out;
	FontAtlas atlas;
	// atlas has ascender=0, descender=0 → lineHeight=0, barHeight=2*paddingY=16
	bar.Render(out, "", 0u, atlas, 640u, 20u);

	const float lineHeight = atlas.GetAscender() - atlas.GetDescender();
	const float barHeight = lineHeight + 2.0f * config.paddingY;

	ASSERT_EQ(out.GetQuads().size(), 2u);
	const QuadCommand& background = out.GetQuads()[0];
	EXPECT_FLOAT_EQ(background.x, 0.0f);
	EXPECT_FLOAT_EQ(background.y, 20.0f - barHeight);
	EXPECT_FLOAT_EQ(background.width, 640.0f);
	EXPECT_FLOAT_EQ(background.height, barHeight);
	ExpectColorEq(background.color, config.background);
}

TEST(CommandBar, test_render_appends_text_at_padded_baseline)
{
	CommandBar bar;
	CommandBarConfig config;
	config.paddingX = 9.5f;
	config.paddingY = 5.0f;
	config.foreground = {0.25f, 0.5f, 0.75f, 1.0f};
	bar.SetConfig(config);

	DrawList out;
	FontAtlas atlas;
	const std::string_view text = "hello utf8";
	const uint32_t windowHeight = 100u;
	bar.Render(out, text, 0u, atlas, 300u, windowHeight);

	const float lineHeight = atlas.GetAscender() - atlas.GetDescender();
	const float barHeight = lineHeight + 2.0f * config.paddingY;
	const float barTop = static_cast<float>(windowHeight) - barHeight;
	const float expectedBaselineY = barTop + config.paddingY + atlas.GetAscender();

	ASSERT_EQ(out.GetTexts().size(), 1u);
	ASSERT_EQ(out.GetTextBuffer(), text);

	const TextCommand& command = out.GetTexts()[0];
	EXPECT_FLOAT_EQ(command.x, config.paddingX);
	EXPECT_FLOAT_EQ(command.y, expectedBaselineY);
	EXPECT_EQ(command.textOffset, 0u);
	EXPECT_EQ(command.textLength, text.size());
	ExpectColorEq(command.color, config.foreground);
}

TEST(CommandBar, test_render_appends_cursor_block_at_cumulative_advance)
{
	CommandBar bar;
	CommandBarConfig config;
	config.paddingX = 7.0f;
	config.paddingY = 3.0f;
	config.cursorColor = {0.8f, 0.4f, 0.2f, 0.6f};
	bar.SetConfig(config);

	DrawList out;
	FontAtlas atlas;
	const std::string_view text = "ab";
	const std::size_t cursorByteIndex = text.size() + 10u;
	const uint32_t windowHeight = 80u;
	bar.Render(out, text, cursorByteIndex, atlas, 200u, windowHeight);

	const float lineHeight = atlas.GetAscender() - atlas.GetDescender();
	const float barHeight = lineHeight + 2.0f * config.paddingY;
	const float barTop = static_cast<float>(windowHeight) - barHeight;
	const float expectedCursorX = ComputeCursorAdvance(text, cursorByteIndex, atlas);
	const float expectedCursorWidth = ComputeExpectedCursorWidth(text, cursorByteIndex, atlas);

	ASSERT_EQ(out.GetQuads().size(), 2u);
	const QuadCommand& cursor = out.GetQuads()[1];
	EXPECT_FLOAT_EQ(cursor.x, config.paddingX + expectedCursorX);
	EXPECT_FLOAT_EQ(cursor.y, barTop + (barHeight - lineHeight) * 0.5f);
	EXPECT_FLOAT_EQ(cursor.width, expectedCursorWidth);
	EXPECT_FLOAT_EQ(cursor.height, lineHeight);
	ExpectColorEq(cursor.color, config.cursorColor);
}

TEST(CommandBar, test_render_consumes_atlas_by_reference_for_duration_of_call)
{
	CommandBar bar;
	CommandBarConfig config;
	config.paddingX = 4.0f;
	config.paddingY = 5.0f;
	config.background = {0.11f, 0.22f, 0.33f, 0.44f};
	config.foreground = {0.55f, 0.66f, 0.77f, 0.88f};
	config.cursorColor = {0.12f, 0.23f, 0.34f, 0.45f};
	bar.SetConfig(config);

	DrawList out;
	out.AddQuad(QuadCommand{1.0f, 2.0f, 3.0f, 4.0f, {0.9f, 0.8f, 0.7f, 0.6f}});
	out.AddText(5.0f, 6.0f, "prefix", {0.1f, 0.2f, 0.3f, 0.4f});

	const std::size_t initialQuadCount = out.GetQuads().size();
	const std::size_t initialTextCount = out.GetTexts().size();
	const std::size_t initialTextBufferSize = out.GetTextBuffer().size();

	const std::string_view text = "cmd";
	const std::size_t cursorByteIndex = 1u;
	const uint32_t windowWidth = 400u;
	const uint32_t windowHeight = 90u;

	{
		FontAtlas atlas;
		bar.Render(out, text, cursorByteIndex, atlas, windowWidth, windowHeight);
	}

	const FontAtlas atlas;
	const float lineHeight = atlas.GetAscender() - atlas.GetDescender();
	const float barHeight = lineHeight + 2.0f * config.paddingY;
	const float barTop = static_cast<float>(windowHeight) - barHeight;
	const float expectedBaselineY = barTop + config.paddingY + atlas.GetAscender();

	ASSERT_EQ(out.GetQuads().size(), initialQuadCount + 2u);
	ASSERT_EQ(out.GetTexts().size(), initialTextCount + 1u);
	ASSERT_EQ(out.GetTextBuffer().size(), initialTextBufferSize + text.size());
	EXPECT_EQ(out.GetTextBuffer().substr(initialTextBufferSize), text);

	const QuadCommand& background = out.GetQuads()[initialQuadCount];
	const TextCommand& textCommand = out.GetTexts()[initialTextCount];
	const QuadCommand& cursor = out.GetQuads()[initialQuadCount + 1u];

	EXPECT_FLOAT_EQ(background.x, 0.0f);
	EXPECT_FLOAT_EQ(background.y, barTop);
	EXPECT_FLOAT_EQ(background.width, static_cast<float>(windowWidth));
	EXPECT_FLOAT_EQ(background.height, barHeight);
	ExpectColorEq(background.color, config.background);

	EXPECT_FLOAT_EQ(textCommand.x, config.paddingX);
	EXPECT_FLOAT_EQ(textCommand.y, expectedBaselineY);
	EXPECT_EQ(textCommand.textOffset, initialTextBufferSize);
	EXPECT_EQ(textCommand.textLength, text.size());
	ExpectColorEq(textCommand.color, config.foreground);

	const float lineHeight2 = atlas.GetAscender() - atlas.GetDescender();
	const float barHeight2 = lineHeight2 + 2.0f * config.paddingY;
	EXPECT_FLOAT_EQ(cursor.x, config.paddingX + ComputeCursorAdvance(text, cursorByteIndex, atlas));
	EXPECT_FLOAT_EQ(cursor.y, barTop + (barHeight2 - lineHeight2) * 0.5f);
	EXPECT_FLOAT_EQ(cursor.width, ComputeExpectedCursorWidth(text, cursorByteIndex, atlas));
	EXPECT_FLOAT_EQ(cursor.height, lineHeight2);
	ExpectColorEq(cursor.color, config.cursorColor);
}
