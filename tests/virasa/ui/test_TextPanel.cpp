#include <gtest/gtest.h>

#include "virasa/ui/TextPanel.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/Types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

using namespace virasa::ui;

namespace
{

void ExpectColorEq(const Color& actual, const Color& expected)
{
	EXPECT_FLOAT_EQ(actual.r, expected.r);
	EXPECT_FLOAT_EQ(actual.g, expected.g);
	EXPECT_FLOAT_EQ(actual.b, expected.b);
	EXPECT_FLOAT_EQ(actual.a, expected.a);
}

std::string GetTextForCommand(const DrawList& out, const TextCommand& command)
{
	return std::string(out.GetTextBuffer().substr(command.textOffset, command.textLength));
}

} // namespace

TEST(TextPanel, test_text_panel_config_holds_default_panel_appearance)
{
	static_assert(std::is_default_constructible_v<TextPanelConfig>);
	static_assert(std::is_copy_constructible_v<TextPanelConfig>);
	static_assert(std::is_copy_assignable_v<TextPanelConfig>);
	static_assert(std::is_move_constructible_v<TextPanelConfig>);
	static_assert(std::is_move_assignable_v<TextPanelConfig>);
	static_assert(std::is_trivially_destructible_v<TextPanelConfig>);
	static_assert(std::is_standard_layout_v<TextPanelConfig>);
	static_assert(std::is_same_v<decltype(TextPanelConfig::background), Color>);
	static_assert(std::is_same_v<decltype(TextPanelConfig::foreground), Color>);
	static_assert(std::is_same_v<decltype(TextPanelConfig::cursor), Color>);
	static_assert(std::is_same_v<decltype(TextPanelConfig::paddingX), float>);
	static_assert(std::is_same_v<decltype(TextPanelConfig::paddingY), float>);
	static_assert(std::is_same_v<decltype(TextPanelConfig::lineSpacing), float>);
	static_assert(std::is_same_v<decltype(TextPanelConfig::cursorBarWidth), float>);
	static_assert(offsetof(TextPanelConfig, background) == 0u);
	static_assert(offsetof(TextPanelConfig, foreground) == sizeof(Color));
	static_assert(offsetof(TextPanelConfig, cursor) == sizeof(Color) * 2u);
	static_assert(offsetof(TextPanelConfig, paddingX) == sizeof(Color) * 3u);
	static_assert(offsetof(TextPanelConfig, paddingY) == sizeof(Color) * 3u + sizeof(float));
	static_assert(offsetof(TextPanelConfig, lineSpacing) == sizeof(Color) * 3u + sizeof(float) * 2u);
	static_assert(offsetof(TextPanelConfig, cursorBarWidth) ==
		sizeof(Color) * 3u + sizeof(float) * 3u);

	TextPanelConfig config;

	ExpectColorEq(config.background, Color{0.078f, 0.082f, 0.094f, 1.0f});
	ExpectColorEq(config.foreground, Color{0.9f, 0.9f, 0.9f, 1.0f});
	ExpectColorEq(config.cursor, Color{0.9f, 0.9f, 0.9f, 1.0f});
	EXPECT_FLOAT_EQ(config.paddingX, 6.0f);
	EXPECT_FLOAT_EQ(config.paddingY, 4.0f);
	EXPECT_FLOAT_EQ(config.lineSpacing, 1.0f);
	EXPECT_FLOAT_EQ(config.cursorBarWidth, 2.0f);
}

TEST(TextPanel, test_text_panel_is_stateless_with_respect_to_text_buffer)
{
	static_assert(std::is_final_v<TextPanel>);
	static_assert(std::is_default_constructible_v<TextPanel>);
	static_assert(std::is_copy_constructible_v<TextPanel>);
	static_assert(std::is_copy_assignable_v<TextPanel>);
	static_assert(std::is_move_constructible_v<TextPanel>);
	static_assert(std::is_move_assignable_v<TextPanel>);
	static_assert(noexcept(std::declval<TextPanel&>().SetConfig(std::declval<const TextPanelConfig&>())));
	static_assert(noexcept(std::declval<const TextPanel&>().GetConfig()));
	static_assert(std::is_same_v<decltype(std::declval<const TextPanel&>().GetConfig()),
		const TextPanelConfig&>);
	static_assert(std::is_same_v<decltype(std::declval<const TextPanel&>().Render(
		std::declval<DrawList&>(), std::declval<std::string_view>(), std::declval<float>(),
		std::declval<float>(), std::declval<float>(), std::declval<float>(),
		std::declval<const FontAtlas&>(), std::declval<CursorStyle>(), std::declval<std::size_t>())),
		void>);

	TextPanel defaultPanel;
	ExpectColorEq(defaultPanel.GetConfig().background, TextPanelConfig{}.background);
	ExpectColorEq(defaultPanel.GetConfig().foreground, TextPanelConfig{}.foreground);
	ExpectColorEq(defaultPanel.GetConfig().cursor, TextPanelConfig{}.cursor);
	EXPECT_FLOAT_EQ(defaultPanel.GetConfig().paddingX, TextPanelConfig{}.paddingX);
	EXPECT_FLOAT_EQ(defaultPanel.GetConfig().paddingY, TextPanelConfig{}.paddingY);
	EXPECT_FLOAT_EQ(defaultPanel.GetConfig().lineSpacing, TextPanelConfig{}.lineSpacing);
	EXPECT_FLOAT_EQ(defaultPanel.GetConfig().cursorBarWidth, TextPanelConfig{}.cursorBarWidth);

	TextPanelConfig config;
	config.background = {0.2f, 0.3f, 0.4f, 0.5f};
	config.foreground = {0.6f, 0.7f, 0.8f, 0.9f};
	config.cursor = {0.15f, 0.25f, 0.35f, 0.45f};
	config.paddingX = 11.0f;
	config.paddingY = 13.0f;
	config.lineSpacing = 1.75f;
	config.cursorBarWidth = 3.5f;

	TextPanel panel;
	panel.SetConfig(config);

	const TextPanelConfig& storedConfig = panel.GetConfig();
	ExpectColorEq(storedConfig.background, config.background);
	ExpectColorEq(storedConfig.foreground, config.foreground);
	ExpectColorEq(storedConfig.cursor, config.cursor);
	EXPECT_FLOAT_EQ(storedConfig.paddingX, config.paddingX);
	EXPECT_FLOAT_EQ(storedConfig.paddingY, config.paddingY);
	EXPECT_FLOAT_EQ(storedConfig.lineSpacing, config.lineSpacing);
	EXPECT_FLOAT_EQ(storedConfig.cursorBarWidth, config.cursorBarWidth);

	config.background = {1.0f, 0.0f, 0.0f, 1.0f};
	config.foreground = {0.0f, 1.0f, 0.0f, 1.0f};
	config.cursor = {0.0f, 0.0f, 1.0f, 0.5f};
	config.paddingX = -1.0f;
	config.paddingY = -2.0f;
	config.lineSpacing = 9.0f;
	config.cursorBarWidth = 99.0f;

	ExpectColorEq(panel.GetConfig().background, Color{0.2f, 0.3f, 0.4f, 0.5f});
	ExpectColorEq(panel.GetConfig().foreground, Color{0.6f, 0.7f, 0.8f, 0.9f});
	ExpectColorEq(panel.GetConfig().cursor, Color{0.15f, 0.25f, 0.35f, 0.45f});
	EXPECT_FLOAT_EQ(panel.GetConfig().paddingX, 11.0f);
	EXPECT_FLOAT_EQ(panel.GetConfig().paddingY, 13.0f);
	EXPECT_FLOAT_EQ(panel.GetConfig().lineSpacing, 1.75f);
	EXPECT_FLOAT_EQ(panel.GetConfig().cursorBarWidth, 3.5f);

	TextPanel copied = panel;
	ExpectColorEq(copied.GetConfig().background, panel.GetConfig().background);
	ExpectColorEq(copied.GetConfig().foreground, panel.GetConfig().foreground);
	ExpectColorEq(copied.GetConfig().cursor, panel.GetConfig().cursor);
	EXPECT_FLOAT_EQ(copied.GetConfig().paddingX, panel.GetConfig().paddingX);
	EXPECT_FLOAT_EQ(copied.GetConfig().paddingY, panel.GetConfig().paddingY);
	EXPECT_FLOAT_EQ(copied.GetConfig().lineSpacing, panel.GetConfig().lineSpacing);
	EXPECT_FLOAT_EQ(copied.GetConfig().cursorBarWidth, panel.GetConfig().cursorBarWidth);

	TextPanel moved = std::move(copied);
	ExpectColorEq(moved.GetConfig().background, panel.GetConfig().background);
	ExpectColorEq(moved.GetConfig().foreground, panel.GetConfig().foreground);
	ExpectColorEq(moved.GetConfig().cursor, panel.GetConfig().cursor);
	EXPECT_FLOAT_EQ(moved.GetConfig().paddingX, panel.GetConfig().paddingX);
	EXPECT_FLOAT_EQ(moved.GetConfig().paddingY, panel.GetConfig().paddingY);
	EXPECT_FLOAT_EQ(moved.GetConfig().lineSpacing, panel.GetConfig().lineSpacing);
	EXPECT_FLOAT_EQ(moved.GetConfig().cursorBarWidth, panel.GetConfig().cursorBarWidth);

	FontAtlas atlas;
	DrawList first;
	DrawList second;

	panel.Render(first, "same\ntext", 10.0f, 20.0f, 100.0f, 50.0f, atlas, CursorStyle::Insertion, 2u);
	panel.Render(second, "same\ntext", 10.0f, 20.0f, 100.0f, 50.0f, atlas, CursorStyle::Insertion, 2u);

	ASSERT_EQ(first.GetQuads().size(), second.GetQuads().size());
	ASSERT_EQ(first.GetTexts().size(), second.GetTexts().size());
	EXPECT_EQ(first.GetTextBuffer(), second.GetTextBuffer());
	EXPECT_TRUE(first.GetImageQuads().empty());
	EXPECT_TRUE(second.GetImageQuads().empty());

	for (size_t i = 0; i < first.GetQuads().size(); ++i)
	{
		EXPECT_FLOAT_EQ(first.GetQuads()[i].x, second.GetQuads()[i].x);
		EXPECT_FLOAT_EQ(first.GetQuads()[i].y, second.GetQuads()[i].y);
		EXPECT_FLOAT_EQ(first.GetQuads()[i].width, second.GetQuads()[i].width);
		EXPECT_FLOAT_EQ(first.GetQuads()[i].height, second.GetQuads()[i].height);
		ExpectColorEq(first.GetQuads()[i].color, second.GetQuads()[i].color);
	}

	for (size_t i = 0; i < first.GetTexts().size(); ++i)
	{
		EXPECT_FLOAT_EQ(first.GetTexts()[i].x, second.GetTexts()[i].x);
		EXPECT_FLOAT_EQ(first.GetTexts()[i].y, second.GetTexts()[i].y);
		EXPECT_EQ(first.GetTexts()[i].textOffset, second.GetTexts()[i].textOffset);
		EXPECT_EQ(first.GetTexts()[i].textLength, second.GetTexts()[i].textLength);
		ExpectColorEq(first.GetTexts()[i].color, second.GetTexts()[i].color);
	}
}

TEST(TextPanel, test_render_appends_background_quad_at_panel_rect)
{
	TextPanel panel;
	TextPanelConfig config;
	config.background = {0.15f, 0.25f, 0.35f, 0.45f};
	panel.SetConfig(config);

	DrawList out;
	FontAtlas atlas;

	panel.Render(out, "", -3.5f, 7.25f, -10.0f, 0.0f, atlas, CursorStyle::None, 123u);

	ASSERT_EQ(out.GetQuads().size(), 1u);
	const QuadCommand& quad = out.GetQuads()[0];
	EXPECT_FLOAT_EQ(quad.x, -3.5f);
	EXPECT_FLOAT_EQ(quad.y, 7.25f);
	EXPECT_FLOAT_EQ(quad.width, -10.0f);
	EXPECT_FLOAT_EQ(quad.height, 0.0f);
	ExpectColorEq(quad.color, config.background);

	EXPECT_TRUE(out.GetTexts().empty());
	EXPECT_TRUE(out.GetImageQuads().empty());
	EXPECT_TRUE(out.GetTextBuffer().empty());
}

TEST(TextPanel, test_render_splits_text_on_newlines_ignoring_carriage_returns)
{
	TextPanel panel;
	TextPanelConfig config;
	config.paddingX = 2.0f;
	config.paddingY = 3.0f;
	panel.SetConfig(config);

	DrawList out;
	FontAtlas atlas;

	panel.Render(out, "a\r\n\n\rb\rc\n\r", 5.0f, 7.0f, 80.0f, 40.0f, atlas, CursorStyle::None, 0u);

	ASSERT_EQ(out.GetQuads().size(), 1u);
	ASSERT_EQ(out.GetTexts().size(), 2u);
	EXPECT_TRUE(out.GetImageQuads().empty());
	EXPECT_EQ(out.GetTextBuffer(), "abc");

	const auto texts = out.GetTexts();
	EXPECT_EQ(texts[0].textOffset, 0u);
	EXPECT_EQ(texts[0].textLength, 1u);
	EXPECT_EQ(texts[1].textOffset, 1u);
	EXPECT_EQ(texts[1].textLength, 2u);

	EXPECT_EQ(GetTextForCommand(out, texts[0]), "a");
	EXPECT_EQ(GetTextForCommand(out, texts[1]), "bc");
}

TEST(TextPanel, test_render_emits_one_text_command_per_non_empty_line)
{
	TextPanel panel;
	TextPanelConfig config;
	config.foreground = {0.11f, 0.22f, 0.33f, 0.44f};
	config.paddingX = 8.0f;
	config.paddingY = 9.0f;
	config.lineSpacing = 1.5f;
	panel.SetConfig(config);

	DrawList out;
	FontAtlas atlas;

	const float x = 100.0f;
	const float y = 200.0f;
	panel.Render(out, "first\n\nsecond", x, y, 300.0f, 120.0f, atlas, CursorStyle::None, 0u);

	ASSERT_EQ(out.GetTexts().size(), 2u);
	EXPECT_EQ(out.GetTextBuffer(), "firstsecond");

	const float ascender = atlas.GetAscender();
	const float descender = atlas.GetDescender();
	const float lineHeight = ascender - descender;
	const float advance = lineHeight * config.lineSpacing;

	const auto texts = out.GetTexts();

	EXPECT_FLOAT_EQ(texts[0].x, x + config.paddingX);
	EXPECT_FLOAT_EQ(texts[0].y, y + config.paddingY + ascender + 0.0f * advance);
	EXPECT_EQ(texts[0].textOffset, 0u);
	EXPECT_EQ(texts[0].textLength, 5u);
	ExpectColorEq(texts[0].color, config.foreground);
	EXPECT_EQ(GetTextForCommand(out, texts[0]), "first");

	EXPECT_FLOAT_EQ(texts[1].x, x + config.paddingX);
	EXPECT_FLOAT_EQ(texts[1].y, y + config.paddingY + ascender + 2.0f * advance);
	EXPECT_EQ(texts[1].textOffset, 5u);
	EXPECT_EQ(texts[1].textLength, 6u);
	ExpectColorEq(texts[1].color, config.foreground);
	EXPECT_EQ(GetTextForCommand(out, texts[1]), "second");
}

TEST(TextPanel, test_cursor_style_enum_values_in_declared_order)
{
	static_assert(std::is_enum_v<CursorStyle>);
	static_assert(std::is_same_v<std::underlying_type_t<CursorStyle>, uint8_t>);

	EXPECT_EQ(static_cast<uint8_t>(CursorStyle::None), 0u);
	EXPECT_EQ(static_cast<uint8_t>(CursorStyle::Block), 1u);
	EXPECT_EQ(static_cast<uint8_t>(CursorStyle::Insertion), 2u);
	EXPECT_LT(static_cast<uint8_t>(CursorStyle::None), static_cast<uint8_t>(CursorStyle::Block));
	EXPECT_LT(static_cast<uint8_t>(CursorStyle::Block), static_cast<uint8_t>(CursorStyle::Insertion));
}

TEST(TextPanel, test_render_emits_cursor_quad_when_style_is_not_none)
{
	TextPanel panel;
	TextPanelConfig config;
	config.background = {0.05f, 0.06f, 0.07f, 1.0f};
	config.foreground = {0.8f, 0.7f, 0.6f, 1.0f};
	config.cursor = {0.2f, 0.4f, 0.6f, 0.5f};
	config.paddingX = 6.0f;
	config.paddingY = 4.0f;
	config.lineSpacing = 1.25f;
	config.cursorBarWidth = 3.0f;
	panel.SetConfig(config);

	FontAtlas atlas;
	const float x = 10.0f;
	const float y = 20.0f;
	const std::string text = "ab\r\nZ\n";
	const float lineHeight = atlas.GetAscender() - atlas.GetDescender();
	const float advance = lineHeight * config.lineSpacing;
	const float cursorHeight = lineHeight;
	const float glyphA = atlas.GetGlyph(static_cast<uint32_t>('a')).advance;
	const float glyphB = atlas.GetGlyph(static_cast<uint32_t>('b')).advance;
	const float glyphZ = atlas.GetGlyph(static_cast<uint32_t>('Z')).advance;
	const float glyphSpace = atlas.GetGlyph(0x20u).advance;

	DrawList noneOut;
	panel.Render(noneOut, text, x, y, 100.0f, 50.0f, atlas, CursorStyle::None, 999u);
	ASSERT_EQ(noneOut.GetQuads().size(), 1u);
	EXPECT_TRUE(noneOut.GetTexts().size() == 2u);

	DrawList insertionOut;
	panel.Render(insertionOut, text, x, y, 100.0f, 50.0f, atlas, CursorStyle::Insertion, 2u);
	ASSERT_EQ(insertionOut.GetQuads().size(), 2u);
	const QuadCommand& insertionCursor = insertionOut.GetQuads()[1];
	EXPECT_FLOAT_EQ(insertionCursor.x, x + config.paddingX + glyphA + glyphB);
	EXPECT_FLOAT_EQ(insertionCursor.y, y + config.paddingY + 0.0f * advance);
	EXPECT_FLOAT_EQ(insertionCursor.width, config.cursorBarWidth);
	EXPECT_FLOAT_EQ(insertionCursor.height, cursorHeight);
	ExpectColorEq(insertionCursor.color, config.cursor);

	DrawList blockOut;
	panel.Render(blockOut, text, x, y, 100.0f, 50.0f, atlas, CursorStyle::Block, 4u);
	ASSERT_EQ(blockOut.GetQuads().size(), 2u);
	const QuadCommand& blockCursor = blockOut.GetQuads()[1];
	EXPECT_FLOAT_EQ(blockCursor.x, x + config.paddingX);
	EXPECT_FLOAT_EQ(blockCursor.y, y + config.paddingY + 1.0f * advance);
	EXPECT_FLOAT_EQ(blockCursor.width, glyphZ);
	EXPECT_FLOAT_EQ(blockCursor.height, cursorHeight);
	ExpectColorEq(blockCursor.color, config.cursor);

	DrawList endOfTextOut;
	panel.Render(endOfTextOut, text, x, y, 100.0f, 50.0f, atlas, CursorStyle::Block, text.size());
	ASSERT_EQ(endOfTextOut.GetQuads().size(), 2u);
	const QuadCommand& endCursor = endOfTextOut.GetQuads()[1];
	EXPECT_FLOAT_EQ(endCursor.x, x + config.paddingX);
	EXPECT_FLOAT_EQ(endCursor.y, y + config.paddingY + 2.0f * advance);
	EXPECT_FLOAT_EQ(endCursor.width, glyphSpace);
	EXPECT_FLOAT_EQ(endCursor.height, cursorHeight);
	ExpectColorEq(endCursor.color, config.cursor);

	DrawList clampedOut;
	panel.Render(clampedOut, text, x, y, 100.0f, 50.0f, atlas, CursorStyle::Insertion,
		text.size() + 100u);
	ASSERT_EQ(clampedOut.GetQuads().size(), 2u);
	const QuadCommand& clampedCursor = clampedOut.GetQuads()[1];
	EXPECT_FLOAT_EQ(clampedCursor.x, endCursor.x);
	EXPECT_FLOAT_EQ(clampedCursor.y, endCursor.y);
	EXPECT_FLOAT_EQ(clampedCursor.width, config.cursorBarWidth);
	EXPECT_FLOAT_EQ(clampedCursor.height, cursorHeight);
	ExpectColorEq(clampedCursor.color, config.cursor);
}

TEST(TextPanel, test_render_consumes_atlas_by_reference_for_duration_of_call)
{
	TextPanel panel;
	TextPanelConfig config;
	config.background = {0.3f, 0.2f, 0.1f, 1.0f};
	config.foreground = {0.8f, 0.7f, 0.6f, 0.5f};
	config.cursor = {0.4f, 0.5f, 0.6f, 0.7f};
	config.paddingX = 4.0f;
	config.paddingY = 5.0f;
	config.lineSpacing = 2.0f;
	config.cursorBarWidth = 1.5f;
	panel.SetConfig(config);

	DrawList out;
	out.AddQuad({1.0f, 2.0f, 3.0f, 4.0f, {0.9f, 0.8f, 0.7f, 0.6f}});
	out.AddText(9.0f, 10.0f, "seed", {0.1f, 0.2f, 0.3f, 0.4f});
	out.AddImageQuad({11.0f, 12.0f, 13.0f, 14.0f, 0.0f, 0.0f, 1.0f, 1.0f, 2u,
		{1.0f, 1.0f, 1.0f, 1.0f}});

	FontAtlas atlas;
	panel.Render(out, "top\n\nbottom", 20.0f, 30.0f, 40.0f, 50.0f, atlas, CursorStyle::Insertion, 1u);

	ASSERT_EQ(out.GetQuads().size(), 3u);
	ASSERT_EQ(out.GetTexts().size(), 3u);
	ASSERT_EQ(out.GetImageQuads().size(), 1u);
	EXPECT_EQ(out.GetTextBuffer(), "seedtopbottom");

	const auto quads = out.GetQuads();
	EXPECT_FLOAT_EQ(quads[0].x, 1.0f);
	EXPECT_FLOAT_EQ(quads[0].y, 2.0f);
	EXPECT_FLOAT_EQ(quads[0].width, 3.0f);
	EXPECT_FLOAT_EQ(quads[0].height, 4.0f);
	ExpectColorEq(quads[0].color, Color{0.9f, 0.8f, 0.7f, 0.6f});

	EXPECT_FLOAT_EQ(quads[1].x, 20.0f);
	EXPECT_FLOAT_EQ(quads[1].y, 30.0f);
	EXPECT_FLOAT_EQ(quads[1].width, 40.0f);
	EXPECT_FLOAT_EQ(quads[1].height, 50.0f);
	ExpectColorEq(quads[1].color, config.background);

	const float advance = (atlas.GetAscender() - atlas.GetDescender()) * config.lineSpacing;
	const float glyphT = atlas.GetGlyph(static_cast<uint32_t>('t')).advance;
	EXPECT_FLOAT_EQ(quads[2].x, 20.0f + config.paddingX + glyphT);
	EXPECT_FLOAT_EQ(quads[2].y, 30.0f + config.paddingY + 0.0f * advance);
	EXPECT_FLOAT_EQ(quads[2].width, config.cursorBarWidth);
	EXPECT_FLOAT_EQ(quads[2].height, atlas.GetAscender() - atlas.GetDescender());
	ExpectColorEq(quads[2].color, config.cursor);

	const auto texts = out.GetTexts();
	EXPECT_EQ(GetTextForCommand(out, texts[0]), "seed");
	EXPECT_EQ(GetTextForCommand(out, texts[1]), "top");
	EXPECT_EQ(GetTextForCommand(out, texts[2]), "bottom");

	const float ascender = atlas.GetAscender();

	EXPECT_FLOAT_EQ(texts[1].x, 24.0f);
	EXPECT_FLOAT_EQ(texts[1].y, 30.0f + config.paddingY + ascender + 0.0f * advance);
	ExpectColorEq(texts[1].color, config.foreground);

	EXPECT_FLOAT_EQ(texts[2].x, 24.0f);
	EXPECT_FLOAT_EQ(texts[2].y, 30.0f + config.paddingY + ascender + 2.0f * advance);
	ExpectColorEq(texts[2].color, config.foreground);
}
