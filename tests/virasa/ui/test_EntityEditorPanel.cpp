#include <gtest/gtest.h>

#include "virasa/ui/EntityEditorPanel.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/Types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
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

TEST(EntityEditorPanel, test_entity_editor_cell_kind_enum_values_in_declared_order)
{
	static_assert(std::is_enum_v<EntityEditorCellKind>);
	static_assert(std::is_same_v<std::underlying_type_t<EntityEditorCellKind>, uint8_t>);
	static_assert(static_cast<uint8_t>(EntityEditorCellKind::SectionLabel) == 0u);
	static_assert(static_cast<uint8_t>(EntityEditorCellKind::Label) == 1u);
	static_assert(static_cast<uint8_t>(EntityEditorCellKind::Value) == 2u);

	EXPECT_LT(
		static_cast<uint8_t>(EntityEditorCellKind::SectionLabel),
		static_cast<uint8_t>(EntityEditorCellKind::Label));
	EXPECT_LT(
		static_cast<uint8_t>(EntityEditorCellKind::Label),
		static_cast<uint8_t>(EntityEditorCellKind::Value));
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
	ExpectColorEq(config.focusedCellBackground, Color{0.18f, 0.22f, 0.32f, 1.0f});
	EXPECT_FLOAT_EQ(config.paddingX, 6.0f);
	EXPECT_FLOAT_EQ(config.paddingY, 4.0f);
	EXPECT_FLOAT_EQ(config.indentX, 12.0f);
	EXPECT_FLOAT_EQ(config.cellSpacing, 8.0f);
	EXPECT_FLOAT_EQ(config.lineSpacing, 1.0f);
}

TEST(EntityEditorPanel, test_entity_editor_panel_cell_holds_kind_text_and_slot)
{
	static_assert(std::is_default_constructible_v<EntityEditorPanelCell>);
	static_assert(std::is_copy_constructible_v<EntityEditorPanelCell>);
	static_assert(std::is_copy_assignable_v<EntityEditorPanelCell>);
	static_assert(std::is_move_constructible_v<EntityEditorPanelCell>);
	static_assert(std::is_move_assignable_v<EntityEditorPanelCell>);
	static_assert(std::is_trivially_destructible_v<EntityEditorPanelCell>);
	static_assert(std::is_same_v<decltype(EntityEditorPanelCell::kind), EntityEditorCellKind>);
	static_assert(std::is_same_v<decltype(EntityEditorPanelCell::text), std::string_view>);
	static_assert(std::is_same_v<decltype(EntityEditorPanelCell::slotIndex), uint32_t>);

	EntityEditorPanelCell cell;

	EXPECT_EQ(cell.kind, EntityEditorCellKind::Label);
	EXPECT_TRUE(cell.text.empty());
	EXPECT_EQ(cell.slotIndex, 0u);
}

TEST(EntityEditorPanel, test_entity_editor_panel_row_is_span_of_cells)
{
	static_assert(std::is_default_constructible_v<EntityEditorPanelRow>);
	static_assert(std::is_copy_constructible_v<EntityEditorPanelRow>);
	static_assert(std::is_copy_assignable_v<EntityEditorPanelRow>);
	static_assert(std::is_move_constructible_v<EntityEditorPanelRow>);
	static_assert(std::is_move_assignable_v<EntityEditorPanelRow>);
	static_assert(std::is_trivially_destructible_v<EntityEditorPanelRow>);
	static_assert(std::is_same_v<
		decltype(EntityEditorPanelRow::cells),
		std::span<const EntityEditorPanelCell>>);

	EntityEditorPanelRow row;

	EXPECT_TRUE(row.cells.empty());
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
	EXPECT_FLOAT_EQ(defaultConfig.cellSpacing, 8.0f);
	EXPECT_FLOAT_EQ(defaultConfig.lineSpacing, 1.0f);
	ExpectColorEq(defaultConfig.background, Color{0.078f, 0.082f, 0.094f, 1.0f});
	ExpectColorEq(defaultConfig.focusedCellBackground, Color{0.18f, 0.22f, 0.32f, 1.0f});

	EntityEditorPanelConfig customConfig{};
	customConfig.background = {0.2f, 0.3f, 0.4f, 0.5f};
	customConfig.sectionHeaderForeground = {0.6f, 0.7f, 0.8f, 0.9f};
	customConfig.captionForeground = {0.11f, 0.22f, 0.33f, 0.44f};
	customConfig.valueForeground = {0.55f, 0.66f, 0.77f, 0.88f};
	customConfig.focusedCellBackground = {0.15f, 0.16f, 0.17f, 0.18f};
	customConfig.paddingX = 10.0f;
	customConfig.paddingY = 11.0f;
	customConfig.indentX = 12.5f;
	customConfig.cellSpacing = 13.0f;
	customConfig.lineSpacing = 1.75f;

	panel.SetConfig(customConfig);
	const EntityEditorPanelConfig& storedConfig = panel.GetConfig();
	ExpectColorEq(storedConfig.background, customConfig.background);
	ExpectColorEq(storedConfig.sectionHeaderForeground, customConfig.sectionHeaderForeground);
	ExpectColorEq(storedConfig.captionForeground, customConfig.captionForeground);
	ExpectColorEq(storedConfig.valueForeground, customConfig.valueForeground);
	ExpectColorEq(storedConfig.focusedCellBackground, customConfig.focusedCellBackground);
	EXPECT_FLOAT_EQ(storedConfig.paddingX, customConfig.paddingX);
	EXPECT_FLOAT_EQ(storedConfig.paddingY, customConfig.paddingY);
	EXPECT_FLOAT_EQ(storedConfig.indentX, customConfig.indentX);
	EXPECT_FLOAT_EQ(storedConfig.cellSpacing, customConfig.cellSpacing);
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
	const std::array<EntityEditorPanelCell, 1> row0Cells = {{
		{EntityEditorCellKind::SectionLabel, "Transform", 99u},
	}};
	const std::array<EntityEditorPanelCell, 2> row1Cells = {{
		{EntityEditorCellKind::Label, "Position", 0u},
		{EntityEditorCellKind::Value, "(1, 2, 3)", 1u},
	}};
	const std::array<EntityEditorPanelRow, 2> rows = {{
		{std::span<const EntityEditorPanelCell>(row0Cells)},
		{std::span<const EntityEditorPanelCell>(row1Cells)},
	}};

	moved.Render(firstDrawList, rows, std::numeric_limits<std::size_t>::max(),
		std::numeric_limits<std::size_t>::max(), atlas, 1.0f, 2.0f, 300.0f, 200.0f);
	moved.Render(secondDrawList, rows, std::numeric_limits<std::size_t>::max(),
		std::numeric_limits<std::size_t>::max(), atlas, 1.0f, 2.0f, 300.0f, 200.0f);

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

	panel.Render(drawList, rows, std::numeric_limits<std::size_t>::max(),
		std::numeric_limits<std::size_t>::max(), atlas, -12.5f, 7.25f, -30.0f, 0.0f);

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

TEST(EntityEditorPanel, test_render_measures_slot_widths_across_all_label_and_value_cells)
{
	EntityEditorPanel panel;
	EntityEditorPanelConfig config{};
	config.paddingX = 5.0f;
	config.indentX = 11.0f;
	config.cellSpacing = 7.0f;
	panel.SetConfig(config);

	DrawList drawList;
	FontAtlas atlas;

	const std::array<EntityEditorPanelCell, 3> row0Cells = {{
		{EntityEditorCellKind::SectionLabel, "Section", 77u},
		{EntityEditorCellKind::Label, "A", 0u},
		{EntityEditorCellKind::Value, "BC", 1u},
	}};
	const std::array<EntityEditorPanelCell, 2> row1Cells = {{
		{EntityEditorCellKind::Label, "DEF", 0u},
		{EntityEditorCellKind::Value, "G", 1u},
	}};
	const std::array<EntityEditorPanelCell, 2> row2Cells = {{
		{EntityEditorCellKind::Label, "", 0u},
		{EntityEditorCellKind::Value, "HIJ", 1u},
	}};
	const std::array<EntityEditorPanelRow, 3> rows = {{
		{std::span<const EntityEditorPanelCell>(row0Cells)},
		{std::span<const EntityEditorPanelCell>(row1Cells)},
		{std::span<const EntityEditorPanelCell>(row2Cells)},
	}};

	panel.Render(drawList, rows, std::numeric_limits<std::size_t>::max(),
		std::numeric_limits<std::size_t>::max(), atlas, 100.0f, 50.0f, 400.0f, 300.0f);

	ASSERT_EQ(drawList.GetQuads().size(), 1u);
	ASSERT_EQ(drawList.GetTexts().size(), 7u);

	const float widthA = atlas.GetGlyph(static_cast<uint32_t>('A')).advance;
	const float widthBC = atlas.GetGlyph(static_cast<uint32_t>('B')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('C')).advance;
	const float widthDEF = atlas.GetGlyph(static_cast<uint32_t>('D')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('E')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('F')).advance;
	const float widthG = atlas.GetGlyph(static_cast<uint32_t>('G')).advance;
	const float widthHIJ = atlas.GetGlyph(static_cast<uint32_t>('H')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('I')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('J')).advance;
	const float slot0Width = widthDEF > widthA ? widthDEF : widthA;
	const float slot1Width = widthHIJ > widthBC ? widthHIJ : widthBC;
	const float slot0X = 100.0f + config.paddingX + config.indentX;
	const float slot1X = slot0X + slot0Width + config.cellSpacing;

	const auto texts = drawList.GetTexts();
	EXPECT_FLOAT_EQ(texts[1].x, slot0X);
	EXPECT_FLOAT_EQ(texts[2].x, slot1X);
	EXPECT_FLOAT_EQ(texts[3].x, slot0X);
	EXPECT_FLOAT_EQ(texts[4].x, slot1X);
	EXPECT_FLOAT_EQ(texts[5].x, slot0X);
	EXPECT_FLOAT_EQ(texts[6].x, slot1X);
	EXPECT_FLOAT_EQ(slot1X, 100.0f + config.paddingX + config.indentX + slot0Width +
		config.cellSpacing);
	EXPECT_GE(slot0Width, widthA);
	EXPECT_GE(slot0Width, widthDEF);
	EXPECT_GE(slot1Width, widthBC);
	EXPECT_GE(slot1Width, widthG);
	EXPECT_GE(slot1Width, widthHIJ);
}

TEST(EntityEditorPanel, test_render_emits_focused_cell_highlight_quad)
{
	EntityEditorPanel panel;
	EntityEditorPanelConfig config{};
	config.focusedCellBackground = {0.31f, 0.32f, 0.33f, 0.34f};
	config.paddingX = 5.0f;
	config.paddingY = 7.0f;
	config.indentX = 11.0f;
	config.cellSpacing = 7.0f;
	config.lineSpacing = 2.0f;
	panel.SetConfig(config);

	FontAtlas atlas;
	const std::array<EntityEditorPanelCell, 2> row0Cells = {{
		{EntityEditorCellKind::Label, "AB", 0u},
		{EntityEditorCellKind::Value, "C", 1u},
	}};
	const std::array<EntityEditorPanelCell, 2> row1Cells = {{
		{EntityEditorCellKind::Label, "D", 0u},
		{EntityEditorCellKind::Value, "EFG", 1u},
	}};
	const std::array<EntityEditorPanelRow, 2> rows = {{
		{std::span<const EntityEditorPanelCell>(row0Cells)},
		{std::span<const EntityEditorPanelCell>(row1Cells)},
	}};

	DrawList drawList;
	panel.Render(drawList, rows, 1u, 1u, atlas, 20.0f, 30.0f, 200.0f, 100.0f);

	ASSERT_EQ(drawList.GetQuads().size(), 2u);
	const QuadCommand& background = drawList.GetQuads()[0];
	const QuadCommand& highlight = drawList.GetQuads()[1];
	EXPECT_FLOAT_EQ(background.x, 20.0f);
	EXPECT_FLOAT_EQ(background.y, 30.0f);

	const float slot0Width = atlas.GetGlyph(static_cast<uint32_t>('A')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('B')).advance;
	const float slot0X = 20.0f + config.paddingX + config.indentX;
	const float slot1X = slot0X + slot0Width + config.cellSpacing;
	const float advance = (atlas.GetAscender() - atlas.GetDescender()) * config.lineSpacing;

	EXPECT_FLOAT_EQ(highlight.x, slot1X);
	EXPECT_FLOAT_EQ(highlight.y, 30.0f + 1.0f * advance);
	EXPECT_FLOAT_EQ(highlight.width,
		atlas.GetGlyph(static_cast<uint32_t>('E')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('F')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('G')).advance);
	EXPECT_FLOAT_EQ(highlight.height, advance);
	ExpectColorEq(highlight.color, config.focusedCellBackground);

	DrawList noHighlightForLabel;
	panel.Render(noHighlightForLabel, rows, 0u, 0u, atlas, 20.0f, 30.0f, 200.0f, 100.0f);
	EXPECT_EQ(noHighlightForLabel.GetQuads().size(), 1u);

	DrawList noHighlightForSentinel;
	panel.Render(noHighlightForSentinel, rows, std::numeric_limits<std::size_t>::max(),
		std::numeric_limits<std::size_t>::max(), atlas, 20.0f, 30.0f, 200.0f, 100.0f);
	EXPECT_EQ(noHighlightForSentinel.GetQuads().size(), 1u);
}

TEST(EntityEditorPanel, test_render_emits_text_commands_per_cell)
{
	EntityEditorPanel panel;
	EntityEditorPanelConfig config{};
	config.sectionHeaderForeground = {0.9f, 0.8f, 0.7f, 1.0f};
	config.captionForeground = {0.1f, 0.2f, 0.3f, 1.0f};
	config.valueForeground = {0.4f, 0.5f, 0.6f, 1.0f};
	config.paddingX = 5.0f;
	config.paddingY = 7.0f;
	config.indentX = 11.0f;
	config.cellSpacing = 9.0f;
	config.lineSpacing = 2.0f;
	panel.SetConfig(config);

	DrawList drawList;
	FontAtlas atlas;
	const std::array<EntityEditorPanelCell, 1> row0Cells = {{
		{EntityEditorCellKind::SectionLabel, "Transform", 123u},
	}};
	const std::array<EntityEditorPanelCell, 3> row1Cells = {{
		{EntityEditorCellKind::Label, "Position", 0u},
		{EntityEditorCellKind::Value, "(0.000, 1.500, 0.000)", 1u},
		{EntityEditorCellKind::Value, "", 2u},
	}};
	const std::array<EntityEditorPanelCell, 2> row2Cells = {{
		{EntityEditorCellKind::Label, "Rotation", 0u},
		{EntityEditorCellKind::Value, "90", 1u},
	}};
	const std::array<EntityEditorPanelRow, 3> rows = {{
		{std::span<const EntityEditorPanelCell>(row0Cells)},
		{std::span<const EntityEditorPanelCell>(row1Cells)},
		{std::span<const EntityEditorPanelCell>(row2Cells)},
	}};

	const float x = 100.0f;
	const float y = 50.0f;
	panel.Render(drawList, rows, std::numeric_limits<std::size_t>::max(),
		std::numeric_limits<std::size_t>::max(), atlas, x, y, 400.0f, 300.0f);

	ASSERT_EQ(drawList.GetQuads().size(), 1u);
	ASSERT_EQ(drawList.GetTexts().size(), 6u);

	const float ascender = atlas.GetAscender();
	const float descender = atlas.GetDescender();
	const float advance = (ascender - descender) * config.lineSpacing;
	const float row0Y = y + config.paddingY + ascender + 0.0f * advance;
	const float row1Y = y + config.paddingY + ascender + 1.0f * advance;
	const float row2Y = y + config.paddingY + ascender + 2.0f * advance;
	const float sectionX = x + config.paddingX;
	const float slot0Width =
		(atlas.GetGlyph(static_cast<uint32_t>('P')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('o')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('s')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('i')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('t')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('i')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('o')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('n')).advance);
	const float slot0WidthAlt =
		(atlas.GetGlyph(static_cast<uint32_t>('R')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('o')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('t')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('a')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('t')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('i')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('o')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('n')).advance);
	const float slot0X = x + config.paddingX + config.indentX;
	const float slot1X = slot0X + ((slot0Width > slot0WidthAlt) ? slot0Width : slot0WidthAlt) +
		config.cellSpacing;
	const float slot1Width =
		(atlas.GetGlyph(static_cast<uint32_t>('(')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('0')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('.')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('0')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('0')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('0')).advance +
		atlas.GetGlyph(static_cast<uint32_t>(',')).advance +
		atlas.GetGlyph(static_cast<uint32_t>(' ')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('1')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('.')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('5')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('0')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('0')).advance +
		atlas.GetGlyph(static_cast<uint32_t>(',')).advance +
		atlas.GetGlyph(static_cast<uint32_t>(' ')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('0')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('.')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('0')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('0')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('0')).advance +
		atlas.GetGlyph(static_cast<uint32_t>(')')).advance);
	const float slot1WidthAlt =
		atlas.GetGlyph(static_cast<uint32_t>('9')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('0')).advance;
	const float slot2X = slot1X + ((slot1Width > slot1WidthAlt) ? slot1Width : slot1WidthAlt) +
		config.cellSpacing;

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
		slot0X,
		row1Y,
		"Position",
		config.captionForeground);
	ExpectTextCommandMatches(
		drawList,
		texts[2],
		slot1X,
		row1Y,
		"(0.000, 1.500, 0.000)",
		config.valueForeground);
	ExpectTextCommandMatches(
		drawList,
		texts[3],
		slot2X,
		row1Y,
		"",
		config.valueForeground);
	ExpectTextCommandMatches(
		drawList,
		texts[4],
		slot0X,
		row2Y,
		"Rotation",
		config.captionForeground);
	ExpectTextCommandMatches(
		drawList,
		texts[5],
		slot1X,
		row2Y,
		"90",
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
	config.cellSpacing = 6.0f;
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
	const std::array<EntityEditorPanelCell, 1> row0Cells = {{
		{EntityEditorCellKind::SectionLabel, "Stats", 99u},
	}};
	const std::array<EntityEditorPanelCell, 2> row1Cells = {{
		{EntityEditorCellKind::Label, "Health", 0u},
		{EntityEditorCellKind::Value, "100", 1u},
	}};
	const std::array<EntityEditorPanelRow, 2> rows = {{
		{std::span<const EntityEditorPanelCell>(row0Cells)},
		{std::span<const EntityEditorPanelCell>(row1Cells)},
	}};

	panel.Render(drawList, rows, std::numeric_limits<std::size_t>::max(),
		std::numeric_limits<std::size_t>::max(), atlas, 10.0f, 20.0f, 200.0f, 80.0f);

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
	const float labelWidth =
		atlas.GetGlyph(static_cast<uint32_t>('H')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('e')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('a')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('l')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('t')).advance +
		atlas.GetGlyph(static_cast<uint32_t>('h')).advance;
	const float captionX = 10.0f + config.paddingX + config.indentX;
	const float valueX = captionX + labelWidth + config.cellSpacing;

	const auto texts = drawList.GetTexts();
	ExpectTextCommandMatches(drawList, texts[0], 777.0f, 666.0f, "prefix", {0.2f, 0.3f, 0.4f, 0.5f});
	ExpectTextCommandMatches(drawList, texts[1], sectionX, row0Y, "Stats", config.sectionHeaderForeground);
	ExpectTextCommandMatches(drawList, texts[2], captionX, row1Y, "Health", config.captionForeground);
	ExpectTextCommandMatches(drawList, texts[3], valueX, row1Y, "100", config.valueForeground);

	EXPECT_EQ(drawList.GetTextBuffer(), std::string_view("prefixStatsHealth100"));
}
