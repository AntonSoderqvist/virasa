#include <gtest/gtest.h>

#include "virasa/ui/TextPanel.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/Types.h"

#include <type_traits>

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

	TextPanelConfig config;

	EXPECT_FLOAT_EQ(config.background.r, 0.078f);
	EXPECT_FLOAT_EQ(config.background.g, 0.082f);
	EXPECT_FLOAT_EQ(config.background.b, 0.094f);
	EXPECT_FLOAT_EQ(config.background.a, 1.0f);

	EXPECT_FLOAT_EQ(config.foreground.r, 0.9f);
	EXPECT_FLOAT_EQ(config.foreground.g, 0.9f);
	EXPECT_FLOAT_EQ(config.foreground.b, 0.9f);
	EXPECT_FLOAT_EQ(config.foreground.a, 1.0f);

	EXPECT_FLOAT_EQ(config.paddingX, 6.0f);
	EXPECT_FLOAT_EQ(config.paddingY, 4.0f);
	EXPECT_FLOAT_EQ(config.lineSpacing, 1.0f);
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

	TextPanelConfig config;
	config.background = {0.2f, 0.3f, 0.4f, 0.5f};
	config.foreground = {0.6f, 0.7f, 0.8f, 0.9f};
	config.paddingX = 11.0f;
	config.paddingY = 13.0f;
	config.lineSpacing = 1.75f;

	TextPanel panel;
	panel.SetConfig(config);

	const TextPanelConfig& storedConfig = panel.GetConfig();
	ExpectColorEq(storedConfig.background, config.background);
	ExpectColorEq(storedConfig.foreground, config.foreground);
	EXPECT_FLOAT_EQ(storedConfig.paddingX, config.paddingX);
	EXPECT_FLOAT_EQ(storedConfig.paddingY, config.paddingY);
	EXPECT_FLOAT_EQ(storedConfig.lineSpacing, config.lineSpacing);

	config.background = {1.0f, 0.0f, 0.0f, 1.0f};
	config.foreground = {0.0f, 1.0f, 0.0f, 1.0f};
	config.paddingX = -1.0f;
	config.paddingY = -2.0f;
	config.lineSpacing = 9.0f;

	ExpectColorEq(panel.GetConfig().background, Color{0.2f, 0.3f, 0.4f, 0.5f});
	ExpectColorEq(panel.GetConfig().foreground, Color{0.6f, 0.7f, 0.8f, 0.9f});
	EXPECT_FLOAT_EQ(panel.GetConfig().paddingX, 11.0f);
	EXPECT_FLOAT_EQ(panel.GetConfig().paddingY, 13.0f);
	EXPECT_FLOAT_EQ(panel.GetConfig().lineSpacing, 1.75f);

	TextPanel copied = panel;
	ExpectColorEq(copied.GetConfig().background, panel.GetConfig().background);
	ExpectColorEq(copied.GetConfig().foreground, panel.GetConfig().foreground);
	EXPECT_FLOAT_EQ(copied.GetConfig().paddingX, panel.GetConfig().paddingX);
	EXPECT_FLOAT_EQ(copied.GetConfig().paddingY, panel.GetConfig().paddingY);
	EXPECT_FLOAT_EQ(copied.GetConfig().lineSpacing, panel.GetConfig().lineSpacing);

	TextPanel moved = std::move(copied);
	ExpectColorEq(moved.GetConfig().background, panel.GetConfig().background);
	ExpectColorEq(moved.GetConfig().foreground, panel.GetConfig().foreground);
	EXPECT_FLOAT_EQ(moved.GetConfig().paddingX, panel.GetConfig().paddingX);
	EXPECT_FLOAT_EQ(moved.GetConfig().paddingY, panel.GetConfig().paddingY);
	EXPECT_FLOAT_EQ(moved.GetConfig().lineSpacing, panel.GetConfig().lineSpacing);

	FontAtlas atlas;
	DrawList first;
	DrawList second;

	panel.Render(first, "same\ntext", 10.0f, 20.0f, 100.0f, 50.0f, atlas);
	panel.Render(second, "same\ntext", 10.0f, 20.0f, 100.0f, 50.0f, atlas);

	ASSERT_EQ(first.GetQuads().size(), second.GetQuads().size());
	ASSERT_EQ(first.GetTexts().size(), second.GetTexts().size());
	EXPECT_EQ(first.GetTextBuffer(), second.GetTextBuffer());

	ASSERT_EQ(first.GetQuads().size(), 1u);
	EXPECT_FLOAT_EQ(first.GetQuads()[0].x, second.GetQuads()[0].x);
	EXPECT_FLOAT_EQ(first.GetQuads()[0].y, second.GetQuads()[0].y);
	EXPECT_FLOAT_EQ(first.GetQuads()[0].width, second.GetQuads()[0].width);
	EXPECT_FLOAT_EQ(first.GetQuads()[0].height, second.GetQuads()[0].height);
	ExpectColorEq(first.GetQuads()[0].color, second.GetQuads()[0].color);

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

	panel.Render(out, "", -3.5f, 7.25f, -10.0f, 0.0f, atlas);

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

	panel.Render(out, "a\r\n\n\rb\rc\n\r", 5.0f, 7.0f, 80.0f, 40.0f, atlas);

	ASSERT_EQ(out.GetQuads().size(), 1u);
	ASSERT_EQ(out.GetTexts().size(), 2u);
	EXPECT_TRUE(out.GetImageQuads().empty());
	EXPECT_EQ(out.GetTextBuffer(), "abc");

	const auto texts = out.GetTexts();
	EXPECT_EQ(texts[0].textOffset, 0u);
	EXPECT_EQ(texts[0].textLength, 1u);
	EXPECT_EQ(texts[1].textOffset, 1u);
	EXPECT_EQ(texts[1].textLength, 2u);

	EXPECT_EQ(out.GetTextBuffer().substr(texts[0].textOffset, texts[0].textLength), "a");
	EXPECT_EQ(out.GetTextBuffer().substr(texts[1].textOffset, texts[1].textLength), "bc");
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
	panel.Render(out, "first\n\nsecond", x, y, 300.0f, 120.0f, atlas);

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
	EXPECT_EQ(out.GetTextBuffer().substr(texts[0].textOffset, texts[0].textLength), "first");

	EXPECT_FLOAT_EQ(texts[1].x, x + config.paddingX);
	EXPECT_FLOAT_EQ(texts[1].y, y + config.paddingY + ascender + 2.0f * advance);
	EXPECT_EQ(texts[1].textOffset, 5u);
	EXPECT_EQ(texts[1].textLength, 6u);
	ExpectColorEq(texts[1].color, config.foreground);
	EXPECT_EQ(out.GetTextBuffer().substr(texts[1].textOffset, texts[1].textLength), "second");
}

TEST(TextPanel, test_render_consumes_atlas_by_reference_for_duration_of_call)
{
	TextPanel panel;
	TextPanelConfig config;
	config.background = {0.3f, 0.2f, 0.1f, 1.0f};
	config.foreground = {0.8f, 0.7f, 0.6f, 0.5f};
	config.paddingX = 4.0f;
	config.paddingY = 5.0f;
	config.lineSpacing = 2.0f;
	panel.SetConfig(config);

	DrawList out;
	out.AddQuad({1.0f, 2.0f, 3.0f, 4.0f, {0.9f, 0.8f, 0.7f, 0.6f}});
	out.AddText(9.0f, 10.0f, "seed", {0.1f, 0.2f, 0.3f, 0.4f});
	out.AddImageQuad({11.0f, 12.0f, 13.0f, 14.0f, 0.0f, 0.0f, 1.0f, 1.0f, 2u,
		{1.0f, 1.0f, 1.0f, 1.0f}});

	FontAtlas atlas;
	panel.Render(out, "top\n\nbottom", 20.0f, 30.0f, 40.0f, 50.0f, atlas);

	ASSERT_EQ(out.GetQuads().size(), 2u);
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

	const auto texts = out.GetTexts();
	EXPECT_EQ(out.GetTextBuffer().substr(texts[0].textOffset, texts[0].textLength), "seed");
	EXPECT_EQ(out.GetTextBuffer().substr(texts[1].textOffset, texts[1].textLength), "top");
	EXPECT_EQ(out.GetTextBuffer().substr(texts[2].textOffset, texts[2].textLength), "bottom");

	const float ascender = atlas.GetAscender();
	const float advance = (atlas.GetAscender() - atlas.GetDescender()) * config.lineSpacing;

	EXPECT_FLOAT_EQ(texts[1].x, 24.0f);
	EXPECT_FLOAT_EQ(texts[1].y, 30.0f + config.paddingY + ascender + 0.0f * advance);
	ExpectColorEq(texts[1].color, config.foreground);

	EXPECT_FLOAT_EQ(texts[2].x, 24.0f);
	EXPECT_FLOAT_EQ(texts[2].y, 30.0f + config.paddingY + ascender + 2.0f * advance);
	ExpectColorEq(texts[2].color, config.foreground);
}
