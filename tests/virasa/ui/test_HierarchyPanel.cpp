#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "virasa/ecs/World.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/HierarchyPanel.h"
#include "virasa/ui/Types.h"
#include "virasa/window/Events.h"

using namespace virasa;
using namespace virasa::ui;

namespace
{

bool ColorsEqual(const ui::Color& lhs, const ui::Color& rhs)
{
	return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

bool EntityEquals(const ecs::Entity& lhs, const ecs::Entity& rhs)
{
	return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

ecs::World MakeHierarchyWorld(ecs::Entity& rootA, ecs::Entity& childA1, ecs::Entity& childA2,
	ecs::Entity& grandchildA1a, ecs::Entity& rootB)
{
	ecs::World world;
	rootA = world.CreateEntity("RootA");
	childA1 = world.CreateEntity("ChildA1");
	childA2 = world.CreateEntity("ChildA2");
	grandchildA1a = world.CreateEntity("GrandchildA1a");
	rootB = world.CreateEntity("RootB");

	EXPECT_EQ(world.SetParent(childA1, rootA), ecs::EcsError::None);
	EXPECT_EQ(world.SetParent(childA2, rootA), ecs::EcsError::None);
	EXPECT_EQ(world.SetParent(grandchildA1a, childA1), ecs::EcsError::None);

	return world;
}

FontAtlas MakeAtlas()
{
	FontAtlas atlas;
	const auto error = atlas.Initialize("tests/assets/fonts/DejaVuSans.ttf", 16u);
	EXPECT_EQ(error, ui::FontAtlasError::None);
	return atlas;
}

std::string_view TextForCommand(const ui::DrawList& drawList, const ui::TextCommand& command)
{
	const std::string_view buffer = drawList.GetTextBuffer();
	return buffer.substr(command.textOffset, command.textLength);
}

} // namespace

TEST(HierarchyPanel, test_hierarchy_panel_key_result_enum_values_in_declared_order)
{
	static_assert(std::is_enum_v<ui::HierarchyPanelKeyResult>);
	static_assert(std::is_same_v<std::underlying_type_t<ui::HierarchyPanelKeyResult>, uint8_t>);
	static_assert(static_cast<uint8_t>(ui::HierarchyPanelKeyResult::Consumed) == 0u);

	EXPECT_EQ(static_cast<uint8_t>(ui::HierarchyPanelKeyResult::Consumed), 0u);
}

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

TEST(HierarchyPanel, test_hierarchy_panel_owns_cursor_and_expanded_set)
{
	static_assert(std::is_default_constructible_v<ui::HierarchyPanel>);
	static_assert(std::is_copy_constructible_v<ui::HierarchyPanel>);
	static_assert(std::is_move_constructible_v<ui::HierarchyPanel>);
	static_assert(std::is_final_v<ui::HierarchyPanel>);
	static_assert(noexcept(std::declval<ui::HierarchyPanel&>().SetConfig(
		std::declval<const ui::HierarchyPanelConfig&>())));
	static_assert(noexcept(std::declval<const ui::HierarchyPanel&>().GetConfig()));

	ecs::Entity rootA;
	ecs::Entity childA1;
	ecs::Entity childA2;
	ecs::Entity grandchildA1a;
	ecs::Entity rootB;
	ecs::World world = MakeHierarchyWorld(rootA, childA1, childA2, grandchildA1a, rootB);

	ui::HierarchyPanel panel;
	ui::HierarchyPanelConfig config;
	config.paddingX = 42.0f;
	config.lineSpacing = 1.5f;
	config.foreground = {0.1f, 0.2f, 0.3f, 0.4f};

	panel.SetConfig(config);
	const ui::HierarchyPanelConfig& storedConfig = panel.GetConfig();
	EXPECT_FLOAT_EQ(storedConfig.paddingX, 42.0f);
	EXPECT_FLOAT_EQ(storedConfig.lineSpacing, 1.5f);
	EXPECT_TRUE(ColorsEqual(storedConfig.foreground, config.foreground));

	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootA));

	panel.HandleTextInput("g", world);
	panel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootA));

	ui::HierarchyPanel copied = panel;
	EXPECT_FLOAT_EQ(copied.GetConfig().paddingX, 42.0f);
	EXPECT_FLOAT_EQ(copied.GetConfig().lineSpacing, 1.5f);
	EXPECT_TRUE(EntityEquals(copied.GetCursorEntity(world), rootA));

	ui::HierarchyPanel moved = std::move(copied);
	EXPECT_FLOAT_EQ(moved.GetConfig().paddingX, 42.0f);
	EXPECT_FLOAT_EQ(moved.GetConfig().lineSpacing, 1.5f);
	EXPECT_TRUE(EntityEquals(moved.GetCursorEntity(world), rootA));
}

TEST(HierarchyPanel, test_visible_rows_are_dfs_preorder_over_expanded_subtrees)
{
	ecs::Entity rootA;
	ecs::Entity childA1;
	ecs::Entity childA2;
	ecs::Entity grandchildA1a;
	ecs::Entity rootB;
	ecs::World world = MakeHierarchyWorld(rootA, childA1, childA2, grandchildA1a, rootB);

	ui::HierarchyPanel panel;

	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootA));

	panel.HandleTextInput("G", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootB));

	panel.HandleTextInput("k", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootA));

	panel.HandleTextInput("l", world);
	panel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA1));

	panel.HandleTextInput("l", world);
	panel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), grandchildA1a));

	panel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA2));

	panel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootB));
}

TEST(HierarchyPanel, test_handle_key_consumes_without_modification)
{
	ecs::Entity rootA;
	ecs::Entity childA1;
	ecs::Entity childA2;
	ecs::Entity grandchildA1a;
	ecs::Entity rootB;
	ecs::World world = MakeHierarchyWorld(rootA, childA1, childA2, grandchildA1a, rootB);

	ui::HierarchyPanel panel;
	panel.HandleTextInput("l", world);
	panel.HandleTextInput("j", world);
	ASSERT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA1));

	const auto result = panel.HandleKey(KeyCode::Escape, world);

	EXPECT_EQ(result, ui::HierarchyPanelKeyResult::Consumed);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA1));

	panel.HandleTextInput("g", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA1));
	panel.HandleKey(KeyCode::Enter, world);
	panel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA2));
}

TEST(HierarchyPanel, test_handle_text_input_dispatches_by_codepoint)
{
	ecs::Entity rootA;
	ecs::Entity childA1;
	ecs::Entity childA2;
	ecs::Entity grandchildA1a;
	ecs::Entity rootB;
	ecs::World world = MakeHierarchyWorld(rootA, childA1, childA2, grandchildA1a, rootB);

	ui::HierarchyPanel panel;

	EXPECT_EQ(panel.HandleTextInput("l", world), ui::HierarchyPanelKeyResult::Consumed);
	EXPECT_EQ(panel.HandleTextInput("j", world), ui::HierarchyPanelKeyResult::Consumed);
	EXPECT_EQ(panel.HandleTextInput("x", world), ui::HierarchyPanelKeyResult::Consumed);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA1));

	EXPECT_EQ(panel.HandleTextInput("gG", world), ui::HierarchyPanelKeyResult::Consumed);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootB));

	EXPECT_EQ(panel.HandleTextInput("gj", world), ui::HierarchyPanelKeyResult::Consumed);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootB));
}

TEST(HierarchyPanel, test_hierarchy_text_input_h_collapses_or_walks_to_parent)
{
	ecs::Entity rootA;
	ecs::Entity childA1;
	ecs::Entity childA2;
	ecs::Entity grandchildA1a;
	ecs::Entity rootB;
	ecs::World world = MakeHierarchyWorld(rootA, childA1, childA2, grandchildA1a, rootB);

	ui::HierarchyPanel panel;

	panel.HandleTextInput("l", world);
	ASSERT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootA));
	panel.HandleTextInput("h", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootA));
	panel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootB));

	panel.HandleTextInput("k", world);
	panel.HandleTextInput("l", world);
	panel.HandleTextInput("j", world);
	ASSERT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA1));
	panel.HandleTextInput("h", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootA));

	panel.HandleTextInput("j", world);
	panel.HandleTextInput("h", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootA));

	ui::HierarchyPanel emptyPanel;
	ecs::World emptyWorld;
expect_true:
	EXPECT_EQ(emptyPanel.HandleTextInput("h", emptyWorld), ui::HierarchyPanelKeyResult::Consumed);
	EXPECT_TRUE(EntityEquals(emptyPanel.GetCursorEntity(emptyWorld), ecs::Entity::Invalid()));
}

TEST(HierarchyPanel, test_hierarchy_text_input_l_expands_or_walks_to_first_child)
{
	ecs::Entity rootA;
	ecs::Entity childA1;
	ecs::Entity childA2;
	ecs::Entity grandchildA1a;
	ecs::Entity rootB;
	ecs::World world = MakeHierarchyWorld(rootA, childA1, childA2, grandchildA1a, rootB);

	ui::HierarchyPanel panel;

	panel.HandleTextInput("l", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootA));
	panel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA1));

	panel.HandleTextInput("k", world);
	panel.HandleTextInput("l", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA1));

	panel.HandleTextInput("l", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA1));
	panel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), grandchildA1a));

	panel.HandleTextInput("l", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), grandchildA1a));

	ui::HierarchyPanel emptyPanel;
	ecs::World emptyWorld;
	EXPECT_EQ(emptyPanel.HandleTextInput("l", emptyWorld), ui::HierarchyPanelKeyResult::Consumed);
	EXPECT_TRUE(EntityEquals(emptyPanel.GetCursorEntity(emptyWorld), ecs::Entity::Invalid()));
}

TEST(HierarchyPanel, test_hierarchy_text_input_j_moves_cursor_down_one_visible_row)
{
	ecs::Entity rootA;
	ecs::Entity childA1;
	ecs::Entity childA2;
	ecs::Entity grandchildA1a;
	ecs::Entity rootB;
	ecs::World world = MakeHierarchyWorld(rootA, childA1, childA2, grandchildA1a, rootB);

	ui::HierarchyPanel panel;
	panel.HandleTextInput("l", world);

	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootA));
	panel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA1));
	panel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA2));
	panel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootB));
	panel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootB));
}

TEST(HierarchyPanel, test_hierarchy_text_input_k_moves_cursor_up_one_visible_row)
{
	ecs::Entity rootA;
	ecs::Entity childA1;
	ecs::Entity childA2;
	ecs::Entity grandchildA1a;
	ecs::Entity rootB;
	ecs::World world = MakeHierarchyWorld(rootA, childA1, childA2, grandchildA1a, rootB);

	ui::HierarchyPanel panel;
	panel.HandleTextInput("l", world);
	panel.HandleTextInput("G", world);
	ASSERT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootB));

	panel.HandleTextInput("k", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA2));
	panel.HandleTextInput("k", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), childA1));
	panel.HandleTextInput("k", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootA));
	panel.HandleTextInput("k", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootA));
}

TEST(HierarchyPanel, test_hierarchy_text_input_gg_moves_cursor_to_first_visible_row)
{
	ecs::Entity rootA;
	ecs::Entity childA1;
	ecs::Entity childA2;
	ecs::Entity grandchildA1a;
	ecs::Entity rootB;
	ecs::World world = MakeHierarchyWorld(rootA, childA1, childA2, grandchildA1a, rootB);

	ui::HierarchyPanel panel;
	panel.HandleTextInput("l", world);
	panel.HandleTextInput("G", world);
	ASSERT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootB));

	panel.HandleTextInput("g", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootB));
	panel.HandleTextInput("g", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootA));

	panel.HandleTextInput("G", world);
	panel.HandleTextInput("g", world);
	panel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootB));

	ui::HierarchyPanel emptyPanel;
	ecs::World emptyWorld;
	EXPECT_EQ(
		emptyPanel.HandleTextInput("gg", emptyWorld), ui::HierarchyPanelKeyResult::Consumed);
	EXPECT_TRUE(EntityEquals(emptyPanel.GetCursorEntity(emptyWorld), ecs::Entity::Invalid()));
}

TEST(HierarchyPanel, test_hierarchy_text_input_capital_G_moves_cursor_to_last_visible_row)
{
	ecs::Entity rootA;
	ecs::Entity childA1;
	ecs::Entity childA2;
	ecs::Entity grandchildA1a;
	ecs::Entity rootB;
	ecs::World world = MakeHierarchyWorld(rootA, childA1, childA2, grandchildA1a, rootB);

	ui::HierarchyPanel panel;
	panel.HandleTextInput("l", world);
	panel.HandleTextInput("G", world);
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootB));

	ui::HierarchyPanel emptyPanel;
	ecs::World emptyWorld;
	EXPECT_EQ(emptyPanel.HandleTextInput("G", emptyWorld), ui::HierarchyPanelKeyResult::Consumed);
	EXPECT_TRUE(EntityEquals(emptyPanel.GetCursorEntity(emptyWorld), ecs::Entity::Invalid()));
}

TEST(HierarchyPanel, test_get_cursor_entity_returns_entity_at_cursor_row_or_invalid)
{
	ecs::Entity rootA;
	ecs::Entity childA1;
	ecs::Entity childA2;
	ecs::Entity grandchildA1a;
	ecs::Entity rootB;
	ecs::World world = MakeHierarchyWorld(rootA, childA1, childA2, grandchildA1a, rootB);

	const ui::HierarchyPanel panel;
	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootA));

	ui::HierarchyPanel mutablePanel;
	mutablePanel.HandleTextInput("l", world);
	mutablePanel.HandleTextInput("j", world);
	EXPECT_TRUE(EntityEquals(mutablePanel.GetCursorEntity(world), childA1));

	ecs::World emptyWorld;
	EXPECT_TRUE(EntityEquals(mutablePanel.GetCursorEntity(emptyWorld), ecs::Entity::Invalid()));
}

TEST(HierarchyPanel, test_render_appends_background_quad_at_panel_rect)
{
	ecs::World world;
	ui::HierarchyPanel panel;
	ui::HierarchyPanelConfig config;
	config.background = {0.11f, 0.22f, 0.33f, 0.44f};
	panel.SetConfig(config);

	FontAtlas atlas = MakeAtlas();
	ui::DrawList drawList;

	panel.Render(drawList, world, atlas, 10.0f, 20.0f, -30.0f, 0.0f);

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
	ecs::Entity rootA;
	ecs::Entity childA1;
	ecs::Entity childA2;
	ecs::Entity grandchildA1a;
	ecs::Entity rootB;
	ecs::World world = MakeHierarchyWorld(rootA, childA1, childA2, grandchildA1a, rootB);

	ui::HierarchyPanel panel;
	ui::HierarchyPanelConfig config;
	config.paddingY = 7.0f;
	config.lineSpacing = 1.25f;
	config.cursorRowBackground = {0.5f, 0.4f, 0.3f, 0.2f};
	panel.SetConfig(config);
	panel.HandleTextInput("l", world);
	panel.HandleTextInput("j", world);

	FontAtlas atlas = MakeAtlas();
	ui::DrawList drawList;

	const float x = 3.0f;
	const float y = 4.0f;
	const float width = 100.0f;
	const float height = 200.0f;
	const float rowHeight = atlas.GetAscender() - atlas.GetDescender();
	const float advance = rowHeight * config.lineSpacing;

	panel.Render(drawList, world, atlas, x, y, width, height);

	const auto quads = drawList.GetQuads();
	ASSERT_EQ(quads.size(), 2u);
	EXPECT_FLOAT_EQ(quads[1].x, x);
	EXPECT_FLOAT_EQ(quads[1].y, y + config.paddingY + advance);
	EXPECT_FLOAT_EQ(quads[1].width, width);
	EXPECT_FLOAT_EQ(quads[1].height, rowHeight);
	EXPECT_TRUE(ColorsEqual(quads[1].color, config.cursorRowBackground));

	ui::HierarchyPanel emptyPanel;
	ui::DrawList emptyDrawList;
	ecs::World emptyWorld;
	emptyPanel.Render(emptyDrawList, emptyWorld, atlas, x, y, width, height);
	EXPECT_EQ(emptyDrawList.GetQuads().size(), 1u);
}

TEST(HierarchyPanel, test_render_emits_one_text_command_per_visible_row)
{
	ecs::Entity rootA;
	ecs::Entity childA1;
	ecs::Entity childA2;
	ecs::Entity grandchildA1a;
	ecs::Entity rootB;
	ecs::World world = MakeHierarchyWorld(rootA, childA1, childA2, grandchildA1a, rootB);

	ui::HierarchyPanel panel;
	ui::HierarchyPanelConfig config;
	config.paddingX = 8.0f;
	config.paddingY = 5.0f;
	config.indentX = 14.0f;
	config.lineSpacing = 1.5f;
	config.foreground = {0.7f, 0.6f, 0.5f, 1.0f};
	panel.SetConfig(config);
	panel.HandleTextInput("l", world);
	panel.HandleTextInput("j", world);
	panel.HandleTextInput("l", world);
	panel.HandleTextInput("gg", world);

	FontAtlas atlas = MakeAtlas();
	ui::DrawList drawList;

	const float x = 11.0f;
	const float y = 13.0f;
	const float advance = (atlas.GetAscender() - atlas.GetDescender()) * config.lineSpacing;
	const float baselineY0 = y + config.paddingY + atlas.GetAscender();

	panel.Render(drawList, world, atlas, x, y, 300.0f, 400.0f);

	const auto texts = drawList.GetTexts();
	ASSERT_EQ(texts.size(), 5u);

	EXPECT_FLOAT_EQ(texts[0].x, x + config.paddingX);
	EXPECT_FLOAT_EQ(texts[0].y, baselineY0 + 0.0f * advance);
	EXPECT_EQ(TextForCommand(drawList, texts[0]), "RootA");
	EXPECT_TRUE(ColorsEqual(texts[0].color, config.foreground));

	EXPECT_FLOAT_EQ(texts[1].x, x + config.paddingX + config.indentX);
	EXPECT_FLOAT_EQ(texts[1].y, baselineY0 + 1.0f * advance);
	EXPECT_EQ(TextForCommand(drawList, texts[1]), "ChildA1");
	EXPECT_TRUE(ColorsEqual(texts[1].color, config.foreground));

	EXPECT_FLOAT_EQ(texts[2].x, x + config.paddingX + 2.0f * config.indentX);
	EXPECT_FLOAT_EQ(texts[2].y, baselineY0 + 2.0f * advance);
	EXPECT_EQ(TextForCommand(drawList, texts[2]), "GrandchildA1a");
	EXPECT_TRUE(ColorsEqual(texts[2].color, config.foreground));

	EXPECT_FLOAT_EQ(texts[3].x, x + config.paddingX + config.indentX);
	EXPECT_FLOAT_EQ(texts[3].y, baselineY0 + 3.0f * advance);
	EXPECT_EQ(TextForCommand(drawList, texts[3]), "ChildA2");
	EXPECT_TRUE(ColorsEqual(texts[3].color, config.foreground));

	EXPECT_FLOAT_EQ(texts[4].x, x + config.paddingX);
	EXPECT_FLOAT_EQ(texts[4].y, baselineY0 + 4.0f * advance);
	EXPECT_EQ(TextForCommand(drawList, texts[4]), "RootB");
	EXPECT_TRUE(ColorsEqual(texts[4].color, config.foreground));
}

TEST(HierarchyPanel, test_render_consumes_references_for_duration_of_call)
{
	ecs::Entity rootA;
	ecs::Entity childA1;
	ecs::Entity childA2;
	ecs::Entity grandchildA1a;
	ecs::Entity rootB;
	ecs::World world = MakeHierarchyWorld(rootA, childA1, childA2, grandchildA1a, rootB);

	ui::HierarchyPanel panel;
	panel.HandleTextInput("l", world);
	panel.HandleTextInput("G", world);

	FontAtlas atlas = MakeAtlas();
	ui::DrawList drawList;
	drawList.AddQuad({1.0f, 2.0f, 3.0f, 4.0f, {0.9f, 0.8f, 0.7f, 0.6f}});

	panel.Render(drawList, world, atlas, 0.0f, 0.0f, 200.0f, 100.0f);

	EXPECT_EQ(drawList.GetQuads().size(), 3u);
	EXPECT_EQ(drawList.GetTexts().size(), 4u);
	EXPECT_EQ(drawList.GetImageQuads().size(), 0u);

	EXPECT_FLOAT_EQ(drawList.GetQuads()[0].x, 1.0f);
	EXPECT_FLOAT_EQ(drawList.GetQuads()[1].x, 0.0f);
	EXPECT_FLOAT_EQ(drawList.GetQuads()[1].y, 0.0f);
	EXPECT_FLOAT_EQ(drawList.GetQuads()[1].width, 200.0f);
	EXPECT_FLOAT_EQ(drawList.GetQuads()[1].height, 100.0f);
	EXPECT_FLOAT_EQ(drawList.GetQuads()[2].x, 0.0f);

	EXPECT_EQ(TextForCommand(drawList, drawList.GetTexts()[0]), "RootA");
	EXPECT_EQ(TextForCommand(drawList, drawList.GetTexts()[1]), "ChildA1");
	EXPECT_EQ(TextForCommand(drawList, drawList.GetTexts()[2]), "ChildA2");
	EXPECT_EQ(TextForCommand(drawList, drawList.GetTexts()[3]), "RootB");

	EXPECT_TRUE(EntityEquals(panel.GetCursorEntity(world), rootB));
}
