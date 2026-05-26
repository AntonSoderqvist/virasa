#include <gtest/gtest.h>

#include "virasa/ui/EntityEditorPanel.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/Types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
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

void ExpectTextCommandMatches(
	const DrawList& drawList,
	const TextCommand& command,
	float expectedX,
	float expectedY,
	std::string_view expectedText,
	const Color& expectedColor)
{
	EXPECT_FLOAT_EQ(command.x, expectedX);
	EXPECT_FLOAT_EQ(command.y, expectedY);
	EXPECT_EQ(command.textLength, expectedText.size());
	ExpectColorEq(command.color, expectedColor);

	const std::string_view buffer = drawList.GetTextBuffer();
	ASSERT_LE(static_cast<std::size_t>(command.textOffset) +
		static_cast<std::size_t>(command.textLength),
		buffer.size());
	EXPECT_EQ(buffer.substr(command.textOffset, command.textLength), expectedText);
}

} // namespace

TEST(EntityEditorPanel, test_entity_editor_row_kind_enum_values_in_declared_order)
{
	static_assert(std::is_enum_v<EntityEditorRowKind>);
	static_assert(std::is_same_v<std::underlying_type_t<EntityEditorRowKind>, uint8_t>);
	static_assert(static_cast<uint8_t>(EntityEditorRowKind::SectionHeader) == 0u);
	static_assert(static_cast<uint8_t>(EntityEditorRowKind::Field) == 1u);

	EXPECT_LT(
		static_cast<uint8_t>(EntityEditorRowKind::SectionHeader),
		static_cast<uint8_t>(EntityEditorRowKind::Field));
}

TEST(EntityEditorPanel, test_entity_editor_panel_config_holds_default_appearance)
{
	static_assert(std::is_default_constructible_v<EntityEditorPanelConfig>);
	static_assert(std::is_copy_constructible_v<EntityEditorPanelConfig>);
	static_assert(std::is_copy_assignable_v<EntityEditorPanelConfig>);
	static_assert(std::is_move_constructible_v<EntityEditorPanelConfig>);
	static_assert(std::is_move_assignable_v<EntityEditorPanelConfig>);
	static_assert(std::is_trivially_destructible_v<EntityEditorPanelConfig>);

	EntityEditorPanelConfig config;

	ExpectColorEq(config.background, Color{0.078f, 0.082f, 0.094f, 1.0f});
	ExpectColorEq(config.sectionHeaderForeground, Color{1.0f, 1.0f, 1.0f, 1.0f});
	ExpectColorEq(config.captionForeground, Color{0.65f, 0.68f, 0.72f, 1.0f});
	ExpectColorEq(config.valueForeground, Color{0.9f, 0.9f, 0.9f, 1.0f});
	EXPECT_FLOAT_EQ(config.paddingX, 6.0f);
	EXPECT_FLOAT_EQ(config.paddingY, 4.0f);
	EXPECT_FLOAT_EQ(config.indentX, 12.0f);
	EXPECT_FLOAT_EQ(config.captionColumnWidth, 120.0f);
	EXPECT_FLOAT_EQ(config.lineSpacing, 1.0f);
}

TEST(EntityEditorPanel, test_entity_editor_panel_row_holds_caption_value_and_kind)
{
	static_assert(std::is_default_constructible_v<EntityEditorPanelRow>);
	static_assert(std::is_copy_constructible_v<EntityEditorPanelRow>);
	static_assert(std::is_copy_assignable_v<EntityEditorPanelRow>);
	static_assert(std::is_move_constructible_v<EntityEditorPanelRow>);
	static_assert(std::is_move_assignable_v<EntityEditorPanelRow>);
	static_assert(std::is_trivially_destructible_v<EntityEditorPanelRow>);
	static_assert(std::is_same_v<decltype(EntityEditorPanelRow::caption), std::string_view>);
	static_assert(std::is_same_v<decltype(EntityEditorPanelRow::value), std::string_view>);
	static_assert(std::is_same_v<decltype(EntityEditorPanelRow::kind), EntityEditorRowKind>);

	EntityEditorPanelRow row;

	EXPECT_TRUE(row.caption.empty());
	EXPECT_TRUE(row.value.empty());
	EXPECT_EQ(row.kind, EntityEditorRowKind::Field);
}

TEST(EntityEditorPanel, test_entity_editor_panel_is_stateless_with_respect_to_visible_rows)
{
	static_assert(std::is_default_constructible_v<EntityEditorPanel>);
	static_assert(std::is_copy_constructible_v<EntityEditorPanel>);
	static_assert(std::is_copy_assignable_v<EntityEditorPanel>);
	static_assert(std::is_move_constructible_v<EntityEditorPanel>);
	static_assert(std::is_move_assignable_v<EntityEditorPanel>);
	static_assert(std::is_final_v<EntityEditorPanel>);
	static_assert(noexcept(std::declval<EntityEditorPanel&>().SetConfig(
		std::declval<const EntityEditorPanelConfig&>())));
	static_assert(noexcept(std::declval<const EntityEditorPanel&>().GetConfig()));
	static_assert(std::is_same_v<
		decltype(std::declval<const EntityEditorPanel&>().GetConfig()),
		const EntityEditorPanelConfig&>);

	EntityEditorPanel panel;
	const EntityEditorPanelConfig& defaultConfig = panel.GetConfig();
	EXPECT_FLOAT_EQ(defaultConfig.paddingX, 6.0f);
	EXPECT_FLOAT_EQ(defaultConfig.paddingY, 4.0f);
	EXPECT_FLOAT_EQ(defaultConfig.indentX, 12.0f);
	EXPECT_FLOAT_EQ(defaultConfig.captionColumnWidth, 120.0f);
	EXPECT_FLOAT_EQ(defaultConfig.lineSpacing, 1.0f);
	ExpectColorEq(defaultConfig.background, Color{0.078f, 0.082f, 0.094f, 1.0f});

	EntityEditorPanelConfig customConfig{};
	customConfig.background = {0.2f, 0.3f, 0.4f, 0.5f};
	customConfig.sectionHeaderForeground = {0.6f, 0.7f, 0.8f, 0.9f};
	customConfig.captionForeground = {0.11f, 0.22f, 0.33f, 0.44f};
	customConfig.valueForeground = {0.55f, 0.66f, 0.77f, 0.88f};
	customConfig.paddingX = 10.0f;
	customConfig.paddingY = 11.0f;
	customConfig.indentX = 12.5f;
	customConfig.captionColumnWidth = 130.0f;
	customConfig.lineSpacing = 1.75f;

	panel.SetConfig(customConfig);
	const EntityEditorPanelConfig& storedConfig = panel.GetConfig();
	ExpectColorEq(storedConfig.background, customConfig.background);
	ExpectColorEq(storedConfig.sectionHeaderForeground, customConfig.sectionHeaderForeground);
	ExpectColorEq(storedConfig.captionForeground, customConfig.captionForeground);
	ExpectColorEq(storedConfig.valueForeground, customConfig.valueForeground);
	EXPECT_FLOAT_EQ(storedConfig.paddingX, customConfig.paddingX);
	EXPECT_FLOAT_EQ(storedConfig.paddingY, customConfig.paddingY);
	EXPECT_FLOAT_EQ(storedConfig.indentX, customConfig.indentX);
	EXPECT_FLOAT_EQ(storedConfig.captionColumnWidth, customConfig.captionColumnWidth);
	EXPECT_FLOAT_EQ(storedConfig.lineSpacing, customConfig.lineSpacing);

	customConfig.paddingX = 999.0f;
	customConfig.background = {1.0f, 0.0f, 0.0f, 1.0f};
	EXPECT_FLOAT_EQ(panel.GetConfig().paddingX, 10.0f);
	ExpectColorEq(panel.GetConfig().background, Color{0.2f, 0.3f, 0.4f, 0.5f});

	EntityEditorPanel copied = panel;
	EXPECT_FLOAT_EQ(copied.GetConfig().paddingX, panel.GetConfig().paddingX);
	EXPECT_FLOAT_EQ(copied.GetConfig().lineSpacing, panel.GetConfig().lineSpacing);
	ExpectColorEq(copied.GetConfig().background, panel.GetConfig().background);

	EntityEditorPanel moved = std::move(copied);
	EXPECT_FLOAT_EQ(moved.GetConfig().paddingX, 10.0f);
	EXPECT_FLOAT_EQ(moved.GetConfig().lineSpacing, 1.75f);
	ExpectColorEq(moved.GetConfig().background, Color{0.2f, 0.3f, 0.4f, 0.5f});

	DrawList firstDrawList;
	DrawList secondDrawList;
	FontAtlas atlas;
	const std::array<EntityEditorPanelRow, 2> rows = {{
		{"Transform", "ignored", EntityEditorRowKind::SectionHeader},
		{"Position", "(1, 2, 3)", EntityEditorRowKind::Field},
	}};

	moved.Render(firstDrawList, rows, atlas, 1.0f, 2.0f, 300.0f, 200.0f);
	moved.Render(secondDrawList, rows, atlas, 1.0f, 2.0f, 300.0f, 200.0f);

	ASSERT_EQ(firstDrawList.GetQuads().size(), secondDrawList.GetQuads().size());
	ASSERT_EQ(firstDrawList.GetTexts().size(), secondDrawList.GetTexts().size());
	EXPECT_EQ(firstDrawList.GetTextBuffer(), secondDrawList.GetTextBuffer());

	for (std::size_t i = 0; i < firstDrawList.GetQuads().size(); ++i)
	{
		const QuadCommand& lhs = firstDrawList.GetQuads()[i];
		const QuadCommand& rhs = secondDrawList.GetQuads()[i];
		EXPECT_FLOAT_EQ(lhs.x, rhs.x);
		EXPECT_FLOAT_EQ(lhs.y, rhs.y);
		EXPECT_FLOAT_EQ(lhs.width, rhs.width);
		EXPECT_FLOAT_EQ(lhs.height, rhs.height);
		ExpectColorEq(lhs.color, rhs.color);
	}

	for (std::size_t i = 0; i < firstDrawList.GetTexts().size(); ++i)
	{
		const TextCommand& lhs = firstDrawList.GetTexts()[i];
		const TextCommand& rhs = secondDrawList.GetTexts()[i];
		EXPECT_FLOAT_EQ(lhs.x, rhs.x);
		EXPECT_FLOAT_EQ(lhs.y, rhs.y);
		EXPECT_EQ(lhs.textOffset, rhs.textOffset);
		EXPECT_EQ(lhs.textLength, rhs.textLength);
		ExpectColorEq(lhs.color, rhs.color);
	}
}

TEST(EntityEditorPanel, test_render_appends_background_quad_at_panel_rect)
{
	EntityEditorPanel panel;
	EntityEditorPanelConfig config{};
	config.background = {0.25f, 0.5f, 0.75f, 1.0f};
	panel.SetConfig(config);

	DrawList drawList;
	FontAtlas atlas;
	const std::array<EntityEditorPanelRow, 0> rows = {};

	panel.Render(drawList, rows, atlas, -12.5f, 7.25f, -30.0f, 0.0f);

	ASSERT_EQ(drawList.GetQuads().size(), 1u);
	const QuadCommand& quad = drawList.GetQuads()[0];
	EXPECT_FLOAT_EQ(quad.x, -12.5f);
	EXPECT_FLOAT_EQ(quad.y, 7.25f);
	EXPECT_FLOAT_EQ(quad.width, -30.0f);
	EXPECT_FLOAT_EQ(quad.height, 0.0f);
	ExpectColorEq(quad.color, config.background);
	EXPECT_TRUE(drawList.GetTexts().empty());
	EXPECT_TRUE(drawList.GetImageQuads().empty());
}

TEST(EntityEditorPanel, test_render_emits_text_commands_per_row)
{
	EntityEditorPanel panel;
	EntityEditorPanelConfig config{};
	config.sectionHeaderForeground = {0.9f, 0.8f, 0.7f, 1.0f};
	config.captionForeground = {0.1f, 0.2f, 0.3f, 1.0f};
	config.valueForeground = {0.4f, 0.5f, 0.6f, 1.0f};
	config.paddingX = 5.0f;
	config.paddingY = 7.0f;
	config.indentX = 11.0f;
	config.captionColumnWidth = 90.0f;
	config.lineSpacing = 2.0f;
	panel.SetConfig(config);

	DrawList drawList;
	FontAtlas atlas;
	const std::array<EntityEditorPanelRow, 3> rows = {{
		{"Transform", "ignored value", EntityEditorRowKind::SectionHeader},
		{"Position", "(0.000, 1.500, 0.000)", EntityEditorRowKind::Field},
		{"Rotation", "", EntityEditorRowKind::Field},
	}};

	const float x = 100.0f;
	const float y = 50.0f;
	panel.Render(drawList, rows, atlas, x, y, 400.0f, 300.0f);

	ASSERT_EQ(drawList.GetQuads().size(), 1u);
	ASSERT_EQ(drawList.GetTexts().size(), 5u);

	const float ascender = atlas.GetAscender();
	const float descender = atlas.GetDescender();
	const float advance = (ascender - descender) * config.lineSpacing;
	const float row0Y = y + config.paddingY + ascender + 0.0f * advance;
	const float row1Y = y + config.paddingY + ascender + 1.0f * advance;
	const float row2Y = y + config.paddingY + ascender + 2.0f * advance;
	const float sectionX = x + config.paddingX;
	const float captionX = x + config.paddingX + config.indentX;
	const float valueX = x + config.paddingX + config.indentX + config.captionColumnWidth;

	const auto texts = drawList.GetTexts();
	ExpectTextCommandMatches(
		drawList,
		texts[0],
		sectionX,
		row0Y,
		"Transform",
		config.sectionHeaderForeground);
	ExpectTextCommandMatches(
		drawList,
		texts[1],
		captionX,
		row1Y,
		"Position",
		config.captionForeground);
	ExpectTextCommandMatches(
		drawList,
		texts[2],
		valueX,
		row1Y,
		"(0.000, 1.500, 0.000)",
		config.valueForeground);
	ExpectTextCommandMatches(
		drawList,
		texts[3],
		captionX,
		row2Y,
		"Rotation",
		config.captionForeground);
	ExpectTextCommandMatches(
		drawList,
		texts[4],
		valueX,
		row2Y,
		"",
		config.valueForeground);
}

TEST(EntityEditorPanel, test_render_consumes_references_for_duration_of_call)
{
	EntityEditorPanel panel;
	EntityEditorPanelConfig config{};
	config.background = {0.12f, 0.23f, 0.34f, 0.45f};
	config.sectionHeaderForeground = {0.91f, 0.82f, 0.73f, 0.64f};
	config.captionForeground = {0.15f, 0.25f, 0.35f, 0.45f};
	config.valueForeground = {0.55f, 0.65f, 0.75f, 0.85f};
	config.paddingX = 3.0f;
	config.paddingY = 2.0f;
	config.indentX = 8.0f;
	config.captionColumnWidth = 44.0f;
	config.lineSpacing = 1.5f;
	panel.SetConfig(config);

	DrawList drawList;
	QuadCommand existingQuad{};
	existingQuad.x = 999.0f;
	existingQuad.y = 998.0f;
	existingQuad.width = 10.0f;
	existingQuad.height = 20.0f;
	existingQuad.color = {1.0f, 0.0f, 0.0f, 1.0f};
	drawList.AddQuad(existingQuad);
	drawList.AddText(777.0f, 666.0f, "prefix", {0.2f, 0.3f, 0.4f, 0.5f});

	FontAtlas atlas;
	const std::array<EntityEditorPanelRow, 2> rows = {{
		{"Stats", "ignored", EntityEditorRowKind::SectionHeader},
		{"Health", "100", EntityEditorRowKind::Field},
	}};

	panel.Render(drawList, rows, atlas, 10.0f, 20.0f, 200.0f, 80.0f);

	ASSERT_EQ(drawList.GetQuads().size(), 2u);
	ASSERT_EQ(drawList.GetTexts().size(), 4u);
	EXPECT_TRUE(drawList.GetImageQuads().empty());

	const QuadCommand& background = drawList.GetQuads()[1];
	EXPECT_FLOAT_EQ(background.x, 10.0f);
	EXPECT_FLOAT_EQ(background.y, 20.0f);
	EXPECT_FLOAT_EQ(background.width, 200.0f);
	EXPECT_FLOAT_EQ(background.height, 80.0f);
	ExpectColorEq(background.color, config.background);

	const float ascender = atlas.GetAscender();
	const float descender = atlas.GetDescender();
	const float advance = (ascender - descender) * config.lineSpacing;
	const float row0Y = 20.0f + config.paddingY + ascender;
	const float row1Y = row0Y + advance;
	const float sectionX = 10.0f + config.paddingX;
	const float captionX = 10.0f + config.paddingX + config.indentX;
	const float valueX = 10.0f + config.paddingX + config.indentX + config.captionColumnWidth;

	const auto texts = drawList.GetTexts();
	ExpectTextCommandMatches(drawList, texts[0], 777.0f, 666.0f, "prefix", {0.2f, 0.3f, 0.4f, 0.5f});
	ExpectTextCommandMatches(drawList, texts[1], sectionX, row0Y, "Stats", config.sectionHeaderForeground);
	ExpectTextCommandMatches(drawList, texts[2], captionX, row1Y, "Health", config.captionForeground);
	ExpectTextCommandMatches(drawList, texts[3], valueX, row1Y, "100", config.valueForeground);

	EXPECT_EQ(drawList.GetTextBuffer(), std::string_view("prefixStatsHealth100"));
}
