#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/HierarchyPanel.h"
#include "virasa/ui/Types.h"

using namespace virasa;
using namespace virasa::ui;

namespace
{

bool ColorsEqual(const ui::Color& lhs, const ui::Color& rhs)
{
	return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

ui::FontAtlas MakeAtlas()
{
	ui::FontAtlas atlas;
	const auto error = atlas.Initialize(
		"../../../../vendor/JetBrainsMono/fonts/ttf/JetBrainsMono-Regular.ttf", 16u);
	EXPECT_EQ(error, ui::FontAtlasError::None);
	return atlas;
}

std::string_view TextForCommand(const ui::DrawList& drawList, const ui::TextCommand& command)
{
	const std::string_view buffer = drawList.GetTextBuffer();
	return buffer.substr(command.textOffset, command.textLength);
}

} // namespace



TEST(HierarchyPanel, test_hierarchy_panel_config_holds_default_appearance)
{
	static_assert(std::is_default_constructible_v<ui::HierarchyPanelConfig>);
	static_assert(std::is_copy_constructible_v<ui::HierarchyPanelConfig>);
	static_assert(std::is_move_constructible_v<ui::HierarchyPanelConfig>);
	static_assert(std::is_trivially_destructible_v<ui::HierarchyPanelConfig>);

	ui::HierarchyPanelConfig config;

	EXPECT_FLOAT_EQ(config.background.r, 0.078f);
	EXPECT_FLOAT_EQ(config.background.g, 0.082f);
	EXPECT_FLOAT_EQ(config.background.b, 0.094f);
	EXPECT_FLOAT_EQ(config.background.a, 1.0f);

	EXPECT_FLOAT_EQ(config.foreground.r, 0.9f);
	EXPECT_FLOAT_EQ(config.foreground.g, 0.9f);
	EXPECT_FLOAT_EQ(config.foreground.b, 0.9f);
	EXPECT_FLOAT_EQ(config.foreground.a, 1.0f);

	EXPECT_FLOAT_EQ(config.cursorRowBackground.r, 0.20f);
	EXPECT_FLOAT_EQ(config.cursorRowBackground.g, 0.30f);
	EXPECT_FLOAT_EQ(config.cursorRowBackground.b, 0.40f);
	EXPECT_FLOAT_EQ(config.cursorRowBackground.a, 1.0f);

	EXPECT_FLOAT_EQ(config.paddingX, 6.0f);
	EXPECT_FLOAT_EQ(config.paddingY, 4.0f);
	EXPECT_FLOAT_EQ(config.indentX, 12.0f);
	EXPECT_FLOAT_EQ(config.lineSpacing, 1.0f);
}

TEST(HierarchyPanel, test_hierarchy_panel_row_holds_name_view_and_depth)
{
	static_assert(std::is_default_constructible_v<ui::HierarchyPanelRow>);
	static_assert(std::is_copy_constructible_v<ui::HierarchyPanelRow>);
	static_assert(std::is_move_constructible_v<ui::HierarchyPanelRow>);
	static_assert(std::is_trivially_destructible_v<ui::HierarchyPanelRow>);

	ui::HierarchyPanelRow row;
	EXPECT_EQ(row.name, std::string_view{});
	EXPECT_EQ(row.depth, 0u);

	std::string_view sv = "hello";
	row.name = sv;
	row.depth = 3u;
	EXPECT_EQ(row.name, sv);
	EXPECT_EQ(row.depth, 3u);

	ui::HierarchyPanelRow copy = row;
	EXPECT_EQ(copy.name, sv);
	EXPECT_EQ(copy.depth, 3u);

	ui::HierarchyPanelRow moved = std::move(copy);
	EXPECT_EQ(moved.name, sv);
	EXPECT_EQ(moved.depth, 3u);
}

TEST(HierarchyPanel, test_hierarchy_panel_is_stateless_with_respect_to_visible_rows)
{
	static_assert(std::is_default_constructible_v<ui::HierarchyPanel>);
	static_assert(std::is_copy_constructible_v<ui::HierarchyPanel>);
	static_assert(std::is_move_constructible_v<ui::HierarchyPanel>);
	static_assert(std::is_final_v<ui::HierarchyPanel>);
	static_assert(noexcept(std::declval<ui::HierarchyPanel&>().SetConfig(
		std::declval<const ui::HierarchyPanelConfig&>())));
	static_assert(noexcept(std::declval<const ui::HierarchyPanel&>().GetConfig()));

	// Default-constructed panel has default config.
	ui::HierarchyPanel panel;
	const ui::HierarchyPanelConfig& defaultConfig = panel.GetConfig();
	EXPECT_FLOAT_EQ(defaultConfig.paddingX, 6.0f);
	EXPECT_FLOAT_EQ(defaultConfig.lineSpacing, 1.0f);

	// SetConfig replaces the stored config.
	ui::HierarchyPanelConfig config;
	config.paddingX = 42.0f;
	config.lineSpacing = 1.5f;
	config.foreground = {0.1f, 0.2f, 0.3f, 0.4f};
	panel.SetConfig(config);
	const ui::HierarchyPanelConfig& storedConfig = panel.GetConfig();
	EXPECT_FLOAT_EQ(storedConfig.paddingX, 42.0f);
	EXPECT_FLOAT_EQ(storedConfig.lineSpacing, 1.5f);
	EXPECT_TRUE(ColorsEqual(storedConfig.foreground, config.foreground));

	// Copies and moves transfer the configuration by value.
	ui::HierarchyPanel copied = panel;
	EXPECT_FLOAT_EQ(copied.GetConfig().paddingX, 42.0f);
	EXPECT_FLOAT_EQ(copied.GetConfig().lineSpacing, 1.5f);
	EXPECT_TRUE(ColorsEqual(copied.GetConfig().foreground, config.foreground));

	ui::HierarchyPanel moved = std::move(copied);
	EXPECT_FLOAT_EQ(moved.GetConfig().paddingX, 42.0f);
	EXPECT_FLOAT_EQ(moved.GetConfig().lineSpacing, 1.5f);
	EXPECT_TRUE(ColorsEqual(moved.GetConfig().foreground, config.foreground));

	// Render is const and calling it N times with identical inputs produces identical output.
	ui::FontAtlas atlas = MakeAtlas();
	std::string_view nameA = "Alpha";
	std::string_view nameB = "Beta";
	const ui::HierarchyPanelRow rows[] = {{nameA, 0u}, {nameB, 1u}};
	const std::span<const ui::HierarchyPanelRow> rowSpan{rows, 2};

	ui::DrawList dl1;
	ui::DrawList dl2;
	const ui::HierarchyPanel constPanel = panel;
	constPanel.Render(dl1, rowSpan, 0u, atlas, 0.0f, 0.0f, 200.0f, 100.0f);
	constPanel.Render(dl2, rowSpan, 0u, atlas, 0.0f, 0.0f, 200.0f, 100.0f);

	const auto quads1 = dl1.GetQuads();
	const auto quads2 = dl2.GetQuads();
	ASSERT_EQ(quads1.size(), quads2.size());
	for (std::size_t i = 0; i < quads1.size(); ++i)
	{
		EXPECT_FLOAT_EQ(quads1[i].x, quads2[i].x);
		EXPECT_FLOAT_EQ(quads1[i].y, quads2[i].y);
		EXPECT_FLOAT_EQ(quads1[i].width, quads2[i].width);
		EXPECT_FLOAT_EQ(quads1[i].height, quads2[i].height);
	}
	const auto texts1 = dl1.GetTexts();
	const auto texts2 = dl2.GetTexts();
	ASSERT_EQ(texts1.size(), texts2.size());
	for (std::size_t i = 0; i < texts1.size(); ++i)
	{
		EXPECT_FLOAT_EQ(texts1[i].x, texts2[i].x);
		EXPECT_FLOAT_EQ(texts1[i].y, texts2[i].y);
		EXPECT_EQ(texts1[i].textLength, texts2[i].textLength);
	}
}





















TEST(HierarchyPanel, test_render_appends_background_quad_at_panel_rect)
{
	ui::HierarchyPanel panel;
	ui::HierarchyPanelConfig config;
	config.background = {0.11f, 0.22f, 0.33f, 0.44f};
	panel.SetConfig(config);

	ui::FontAtlas atlas = MakeAtlas();
	ui::DrawList drawList;

	// Empty rows span, no cursor highlight expected.
	const std::span<const ui::HierarchyPanelRow> emptyRows{};
	panel.Render(drawList, emptyRows, 0u, atlas, 10.0f, 20.0f, -30.0f, 0.0f);

	const auto quads = drawList.GetQuads();
	ASSERT_EQ(quads.size(), 1u);
	EXPECT_FLOAT_EQ(quads[0].x, 10.0f);
	EXPECT_FLOAT_EQ(quads[0].y, 20.0f);
	EXPECT_FLOAT_EQ(quads[0].width, -30.0f);
	EXPECT_FLOAT_EQ(quads[0].height, 0.0f);
	EXPECT_TRUE(ColorsEqual(quads[0].color, config.background));
}

TEST(HierarchyPanel, test_render_appends_cursor_row_highlight_quad)
{
	ui::HierarchyPanel panel;
	ui::HierarchyPanelConfig config;
	config.paddingY = 7.0f;
	config.lineSpacing = 1.25f;
	config.cursorRowBackground = {0.5f, 0.4f, 0.3f, 0.2f};
	panel.SetConfig(config);

	ui::FontAtlas atlas = MakeAtlas();
	ui::DrawList drawList;

	const float x = 3.0f;
	const float y = 4.0f;
	const float width = 100.0f;
	const float height = 200.0f;
	const float rowHeight = atlas.GetAscender() - atlas.GetDescender();
	const float advance = rowHeight * config.lineSpacing;

	// cursorRow=1 with 3 rows: highlight should be at row index 1.
	const ui::HierarchyPanelRow rows[] = {
		{"Alpha", 0u},
		{"Beta", 1u},
		{"Gamma", 0u},
	};
	const std::span<const ui::HierarchyPanelRow> rowSpan{rows, 3};
	panel.Render(drawList, rowSpan, 1u, atlas, x, y, width, height);

	const auto quads = drawList.GetQuads();
	ASSERT_EQ(quads.size(), 2u);
	EXPECT_FLOAT_EQ(quads[1].x, x);
	EXPECT_FLOAT_EQ(quads[1].y, y + config.paddingY + 1u * advance);
	EXPECT_FLOAT_EQ(quads[1].width, width);
	EXPECT_FLOAT_EQ(quads[1].height, rowHeight);
	EXPECT_TRUE(ColorsEqual(quads[1].color, config.cursorRowBackground));

	// Empty rows: no highlight quad.
	ui::DrawList emptyDrawList;
	const std::span<const ui::HierarchyPanelRow> emptyRows{};
	panel.Render(emptyDrawList, emptyRows, 0u, atlas, x, y, width, height);
	EXPECT_EQ(emptyDrawList.GetQuads().size(), 1u);

	// cursorRow beyond n-1 is clamped to n-1.
	ui::DrawList clampedDrawList;
	const ui::HierarchyPanelRow twoRows[] = {{"X", 0u}, {"Y", 0u}};
	const std::span<const ui::HierarchyPanelRow> twoRowSpan{twoRows, 2};
	panel.Render(clampedDrawList, twoRowSpan, 999u, atlas, x, y, width, height);
	const auto clampedQuads = clampedDrawList.GetQuads();
	ASSERT_EQ(clampedQuads.size(), 2u);
	// Clamped to index 1 (last row).
	EXPECT_FLOAT_EQ(clampedQuads[1].y, y + config.paddingY + 1u * advance);
}

TEST(HierarchyPanel, test_render_emits_one_text_command_per_visible_row)
{
	ui::HierarchyPanel panel;
	ui::HierarchyPanelConfig config;
	config.paddingX = 8.0f;
	config.paddingY = 5.0f;
	config.indentX = 14.0f;
	config.lineSpacing = 1.5f;
	config.foreground = {0.7f, 0.6f, 0.5f, 1.0f};
	panel.SetConfig(config);

	ui::FontAtlas atlas = MakeAtlas();
	ui::DrawList drawList;

	const float x = 11.0f;
	const float y = 13.0f;
	const float advance = (atlas.GetAscender() - atlas.GetDescender()) * config.lineSpacing;
	const float baselineY0 = y + config.paddingY + atlas.GetAscender();

	// Rows: depth 0, 1, 2, 1, 0 — mimicking a small hierarchy.
	const std::string_view nameA = "RootA";
	const std::string_view nameB = "ChildA1";
	const std::string_view nameC = "GrandchildA1a";
	const std::string_view nameD = "ChildA2";
	const std::string_view nameE = "RootB";
	const ui::HierarchyPanelRow rows[] = {
		{nameA, 0u},
		{nameB, 1u},
		{nameC, 2u},
		{nameD, 1u},
		{nameE, 0u},
	};
	const std::span<const ui::HierarchyPanelRow> rowSpan{rows, 5};
	panel.Render(drawList, rowSpan, 0u, atlas, x, y, 300.0f, 400.0f);

	const auto texts = drawList.GetTexts();
	ASSERT_EQ(texts.size(), 5u);

	EXPECT_FLOAT_EQ(texts[0].x, x + config.paddingX + 0u * config.indentX);
	EXPECT_FLOAT_EQ(texts[0].y, baselineY0 + 0.0f * advance);
	EXPECT_EQ(TextForCommand(drawList, texts[0]), nameA);
	EXPECT_TRUE(ColorsEqual(texts[0].color, config.foreground));

	EXPECT_FLOAT_EQ(texts[1].x, x + config.paddingX + 1u * config.indentX);
	EXPECT_FLOAT_EQ(texts[1].y, baselineY0 + 1.0f * advance);
	EXPECT_EQ(TextForCommand(drawList, texts[1]), nameB);
	EXPECT_TRUE(ColorsEqual(texts[1].color, config.foreground));

	EXPECT_FLOAT_EQ(texts[2].x, x + config.paddingX + 2u * config.indentX);
	EXPECT_FLOAT_EQ(texts[2].y, baselineY0 + 2.0f * advance);
	EXPECT_EQ(TextForCommand(drawList, texts[2]), nameC);
	EXPECT_TRUE(ColorsEqual(texts[2].color, config.foreground));

	EXPECT_FLOAT_EQ(texts[3].x, x + config.paddingX + 1u * config.indentX);
	EXPECT_FLOAT_EQ(texts[3].y, baselineY0 + 3.0f * advance);
	EXPECT_EQ(TextForCommand(drawList, texts[3]), nameD);
	EXPECT_TRUE(ColorsEqual(texts[3].color, config.foreground));

	EXPECT_FLOAT_EQ(texts[4].x, x + config.paddingX + 0u * config.indentX);
	EXPECT_FLOAT_EQ(texts[4].y, baselineY0 + 4.0f * advance);
	EXPECT_EQ(TextForCommand(drawList, texts[4]), nameE);
	EXPECT_TRUE(ColorsEqual(texts[4].color, config.foreground));

	// Zero rows: no text commands.
	ui::DrawList emptyDrawList;
	const std::span<const ui::HierarchyPanelRow> emptyRows{};
	panel.Render(emptyDrawList, emptyRows, 0u, atlas, x, y, 300.0f, 400.0f);
	EXPECT_EQ(emptyDrawList.GetTexts().size(), 0u);
}

TEST(HierarchyPanel, test_render_consumes_references_for_duration_of_call)
{
	ui::HierarchyPanel panel;

	ui::FontAtlas atlas = MakeAtlas();

	// Pre-populate the DrawList to verify Render appends (not clears).
	ui::DrawList drawList;
	drawList.AddQuad({1.0f, 2.0f, 3.0f, 4.0f, {0.9f, 0.8f, 0.7f, 0.6f}});

	// 4 rows: background quad + cursor highlight quad + 4 text commands.
	const std::string nameA = "RootA";
	const std::string nameB = "ChildA1";
	const std::string nameC = "ChildA2";
	const std::string nameD = "RootB";
	const ui::HierarchyPanelRow rows[] = {
		{nameA, 0u},
		{nameB, 1u},
		{nameC, 1u},
		{nameD, 0u},
	};
	const std::span<const ui::HierarchyPanelRow> rowSpan{rows, 4};
	// cursorRow = 3 (last row).
	panel.Render(drawList, rowSpan, 3u, atlas, 0.0f, 0.0f, 200.0f, 100.0f);

	// Render appends; does not call Clear.
	// Pre-existing quad is still first.
	const auto quads = drawList.GetQuads();
	ASSERT_GE(quads.size(), 3u);
	EXPECT_FLOAT_EQ(quads[0].x, 1.0f);
	// Background quad appended by Render.
	EXPECT_FLOAT_EQ(quads[1].x, 0.0f);
	EXPECT_FLOAT_EQ(quads[1].y, 0.0f);
	EXPECT_FLOAT_EQ(quads[1].width, 200.0f);
	EXPECT_FLOAT_EQ(quads[1].height, 100.0f);
	// Cursor highlight quad appended after background.
	EXPECT_FLOAT_EQ(quads[2].x, 0.0f);

	// Text commands in input order.
	const auto texts = drawList.GetTexts();
	ASSERT_EQ(texts.size(), 4u);
	EXPECT_EQ(TextForCommand(drawList, texts[0]), nameA);
	EXPECT_EQ(TextForCommand(drawList, texts[1]), nameB);
	EXPECT_EQ(TextForCommand(drawList, texts[2]), nameC);
	EXPECT_EQ(TextForCommand(drawList, texts[3]), nameD);

	// No image quads emitted.
	EXPECT_EQ(drawList.GetImageQuads().size(), 0u);

	// Render does not retain the span or atlas after return.
	// Calling Render again with a fresh DrawList and same inputs produces identical output.
	ui::DrawList drawList2;
	panel.Render(drawList2, rowSpan, 3u, atlas, 0.0f, 0.0f, 200.0f, 100.0f);
	const auto quads2 = drawList2.GetQuads();
	const auto texts2 = drawList2.GetTexts();
	ASSERT_EQ(quads2.size(), 2u);
	ASSERT_EQ(texts2.size(), 4u);
	EXPECT_EQ(TextForCommand(drawList2, texts2[0]), nameA);
	EXPECT_EQ(TextForCommand(drawList2, texts2[1]), nameB);
	EXPECT_EQ(TextForCommand(drawList2, texts2[2]), nameC);
	EXPECT_EQ(TextForCommand(drawList2, texts2[3]), nameD);
}
