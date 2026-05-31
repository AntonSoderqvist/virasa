#include <gtest/gtest.h>

#include "virasa/editor/HierarchyView.h"
#include "virasa/ecs/Components.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/HierarchyPanel.h"
#include "virasa/ui/Types.h"
#include "virasa/window/Events.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace virasa;

namespace
{

ecs::World MakeSampleWorld()
{
	ecs::World world;

	const ecs::Entity rootA = world.CreateEntity("RootA");
	const ecs::Entity rootB = world.CreateEntity("RootB");
	const ecs::Entity childA1 = world.CreateEntity("ChildA1");
	const ecs::Entity childA2 = world.CreateEntity("ChildA2");
	const ecs::Entity grandchildA1 = world.CreateEntity("GrandchildA1");
	const ecs::Entity childB1 = world.CreateEntity("ChildB1");

	EXPECT_EQ(world.SetParent(childA1, rootA), ecs::EcsError::None);
	EXPECT_EQ(world.SetParent(childA2, rootA), ecs::EcsError::None);
	EXPECT_EQ(world.SetParent(grandchildA1, childA1), ecs::EcsError::None);
	EXPECT_EQ(world.SetParent(childB1, rootB), ecs::EcsError::None);

	return world;
}

const ui::TextCommand& FindTextByContent(const ui::DrawList& drawList, std::string_view text)
{
	const std::string_view buffer = drawList.GetTextBuffer();
	for (const ui::TextCommand& command : drawList.GetTexts())
	{
		const std::string_view candidate = buffer.substr(command.textOffset, command.textLength);
		if (candidate == text)
		{
			return command;
		}
	}

	ADD_FAILURE() << "Expected text command not found: " << text;
	return drawList.GetTexts().front();
}

} // namespace

TEST(HierarchyView, test_hierarchy_view_owns_panel_cursor_expanded_and_pending_g)
{
	static_assert(std::is_final_v<editor::HierarchyView>);
	static_assert(std::is_default_constructible_v<editor::HierarchyView>);
	static_assert(std::is_copy_constructible_v<editor::HierarchyView>);
	static_assert(std::is_copy_assignable_v<editor::HierarchyView>);
	static_assert(std::is_move_constructible_v<editor::HierarchyView>);
	static_assert(std::is_move_assignable_v<editor::HierarchyView>);

	editor::HierarchyView view;
	EXPECT_EQ(view.GetCursorRow(), 0u);

	ecs::World emptyWorld;
	EXPECT_EQ(view.GetCursorEntity(emptyWorld), ecs::Entity::Invalid());

	ui::HierarchyPanelConfig config;
	config.paddingX = 17.0f;
	view.GetPanel().SetConfig(config);
	EXPECT_FLOAT_EQ(view.GetPanel().GetConfig().paddingX, 17.0f);

	EXPECT_EQ(view.HandleTextInput("g", emptyWorld), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 0u);
	EXPECT_EQ(view.HandleKey(KeyCode::J, emptyWorld), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 0u);

	editor::HierarchyView copied = view;
	EXPECT_EQ(copied.GetCursorRow(), view.GetCursorRow());
	EXPECT_FLOAT_EQ(copied.GetPanel().GetConfig().paddingX, 17.0f);
	EXPECT_EQ(copied.HandleTextInput("g", emptyWorld), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(copied.GetCursorRow(), 0u);

	editor::HierarchyView moved = std::move(copied);
	EXPECT_EQ(moved.GetCursorRow(), 0u);
	EXPECT_FLOAT_EQ(moved.GetPanel().GetConfig().paddingX, 17.0f);
	EXPECT_EQ(moved.HandleTextInput("g", emptyWorld), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(moved.GetCursorRow(), 0u);
}

TEST(HierarchyView, test_hierarchy_view_key_result_enum_values_in_declared_order)
{
	static_assert(std::is_same_v<std::underlying_type_t<editor::HierarchyViewKeyResult>, uint8_t>);

	EXPECT_EQ(static_cast<uint8_t>(editor::HierarchyViewKeyResult::Consumed), 0u);
	EXPECT_EQ(static_cast<uint8_t>(editor::HierarchyViewKeyResult::RequestCommandBar), 1u);
	EXPECT_EQ(static_cast<uint8_t>(editor::HierarchyViewKeyResult::RequestEntityEditor), 2u);
}

TEST(HierarchyView, test_get_panel_returns_owned_hierarchy_panel)
{
	editor::HierarchyView view;

	ui::HierarchyPanel& panel = view.GetPanel();
	const editor::HierarchyView& constView = view;
	const ui::HierarchyPanel& constPanel = constView.GetPanel();

	EXPECT_EQ(&panel, &constPanel);
	EXPECT_EQ(view.GetCursorRow(), 0u);

	ui::HierarchyPanelConfig config = panel.GetConfig();
	config.indentX = 23.0f;
	panel.SetConfig(config);

	EXPECT_FLOAT_EQ(view.GetPanel().GetConfig().indentX, 23.0f);
	EXPECT_FLOAT_EQ(constView.GetPanel().GetConfig().indentX, 23.0f);
	EXPECT_EQ(view.GetCursorRow(), 0u);
}

TEST(HierarchyView, test_get_cursor_row_returns_current_visible_row_index)
{
	ecs::World world;
	const ecs::Entity root = world.CreateEntity("Root");
	const ecs::Entity child = world.CreateEntity("Child");
	ASSERT_EQ(world.SetParent(child, root), ecs::EcsError::None);

	editor::HierarchyView view;
	EXPECT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 1u);

	world.DestroyEntity(child);
	EXPECT_EQ(view.GetCursorRow(), 1u);
}

TEST(HierarchyView, test_visible_rows_are_dfs_preorder_over_expanded_subtrees)
{
	ecs::World world = MakeSampleWorld();
	const ecs::Entity rootA = world.FindEntityByName("RootA");
	const ecs::Entity childA1 = world.FindEntityByName("ChildA1");
	const ecs::Entity childA2 = world.FindEntityByName("ChildA2");
	const ecs::Entity rootB = world.FindEntityByName("RootB");
	const ecs::Entity childB1 = world.FindEntityByName("ChildB1");
	const ecs::Entity grandchildA1 = world.FindEntityByName("GrandchildA1");

	ASSERT_TRUE(rootA.IsValid());
	ASSERT_TRUE(childA1.IsValid());
	ASSERT_TRUE(childA2.IsValid());
	ASSERT_TRUE(rootB.IsValid());
	ASSERT_TRUE(childB1.IsValid());
	ASSERT_TRUE(grandchildA1.IsValid());

	editor::HierarchyView view;

	EXPECT_EQ(view.GetCursorEntity(world), rootA);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), rootB);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), rootB);

	EXPECT_EQ(view.HandleTextInput("gg", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), rootA);
	EXPECT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), rootA);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), childA1);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), childA2);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), rootB);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), rootB);

	EXPECT_EQ(view.HandleTextInput("gg", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), rootA);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), childA1);
	EXPECT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), childA1);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), grandchildA1);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), childA2);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), rootB);
	EXPECT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), rootB);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), childB1);
}

TEST(HierarchyView, test_get_cursor_entity_returns_entity_at_cursor_row_or_invalid)
{
	editor::HierarchyView emptyView;
	ecs::World emptyWorld;
	EXPECT_EQ(emptyView.GetCursorEntity(emptyWorld), ecs::Entity::Invalid());

	ecs::World world;
	const ecs::Entity root = world.CreateEntity("Root");
	const ecs::Entity child = world.CreateEntity("Child");
	ASSERT_EQ(world.SetParent(child, root), ecs::EcsError::None);

	editor::HierarchyView view;
	EXPECT_EQ(view.GetCursorEntity(world), root);
	EXPECT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), child);

	world.DestroyEntity(child);
	EXPECT_EQ(view.GetCursorEntity(world), ecs::Entity::Invalid());
}

TEST(HierarchyView, test_handle_key_consumes_without_modification)
{
	ecs::World world;
	const ecs::Entity root = world.CreateEntity("Root");
	const ecs::Entity child = world.CreateEntity("Child");
	ASSERT_EQ(world.SetParent(child, root), ecs::EcsError::None);

	editor::HierarchyView view;
	ASSERT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	ASSERT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	ASSERT_EQ(view.GetCursorEntity(world), child);
	ASSERT_EQ(view.GetCursorRow(), 1u);

	EXPECT_EQ(view.HandleTextInput("g", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.HandleKey(KeyCode::Escape, world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 1u);
	EXPECT_EQ(view.GetCursorEntity(world), child);

	EXPECT_EQ(view.HandleTextInput("g", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.HandleKey(KeyCode::Down, world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.HandleTextInput("g", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 1u);

	EXPECT_EQ(view.HandleKey(KeyCode::Escape, world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.HandleTextInput("g", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.HandleKey(KeyCode::Enter, world), editor::HierarchyViewKeyResult::RequestEntityEditor);
	EXPECT_EQ(view.GetCursorRow(), 1u);
	EXPECT_EQ(view.GetCursorEntity(world), child);

	editor::HierarchyView emptyView;
	ecs::World emptyWorld;
	EXPECT_EQ(emptyView.HandleTextInput("g", emptyWorld), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(emptyView.HandleKey(KeyCode::Enter, emptyWorld), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(emptyView.GetCursorRow(), 0u);
	EXPECT_EQ(emptyView.GetCursorEntity(emptyWorld), ecs::Entity::Invalid());
}

TEST(HierarchyView, test_handle_text_input_dispatches_by_codepoint)
{
	ecs::World world;
	const ecs::Entity root = world.CreateEntity("Root");
	const ecs::Entity child = world.CreateEntity("Child");
	ASSERT_EQ(world.SetParent(child, root), ecs::EcsError::None);

	editor::HierarchyView view;

	EXPECT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), root);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), child);
	EXPECT_EQ(view.HandleTextInput("k", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), root);
	EXPECT_EQ(view.HandleTextInput("G", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), child);
	EXPECT_EQ(view.HandleTextInput("gg", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), root);

	EXPECT_EQ(view.HandleTextInput("gx", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), root);
	EXPECT_EQ(view.HandleTextInput("g:", world), editor::HierarchyViewKeyResult::RequestCommandBar);
	EXPECT_EQ(view.GetCursorEntity(world), root);

	EXPECT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 1u);
	world.DestroyEntity(child);
	EXPECT_EQ(view.GetCursorRow(), 1u);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 0u);
	EXPECT_EQ(view.GetCursorEntity(world), root);
}

TEST(HierarchyView, test_hierarchy_text_input_h_collapses_or_walks_to_parent)
{
	ecs::World world;
	const ecs::Entity root = world.CreateEntity("Root");
	const ecs::Entity child = world.CreateEntity("Child");
	const ecs::Entity grandchild = world.CreateEntity("Grandchild");
	ASSERT_EQ(world.SetParent(child, root), ecs::EcsError::None);
	ASSERT_EQ(world.SetParent(grandchild, child), ecs::EcsError::None);

	editor::HierarchyView view;

	// 'h' on a root (no parent) is a no-op
	EXPECT_EQ(view.HandleTextInput("h", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), root);
	EXPECT_EQ(view.GetCursorRow(), 0u);

	// expand root, then 'h' collapses it (cursorRow stays at root)
	EXPECT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), root);
	EXPECT_EQ(view.HandleTextInput("h", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), root);
	EXPECT_EQ(view.GetCursorRow(), 0u);
	// root is now collapsed — 'j' should not move past root (only 1 visible row)
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), root);

	// expand root, move to child, then 'h' walks to parent (root)
	EXPECT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), child);
	EXPECT_EQ(view.HandleTextInput("h", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), root);
	EXPECT_EQ(view.GetCursorRow(), 0u);

	// expand root and child, move to grandchild, then 'h' collapses child
	EXPECT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed); // expand root
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed); // move to child
	EXPECT_EQ(view.GetCursorEntity(world), child);
	EXPECT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed); // expand child
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed); // move to grandchild
	EXPECT_EQ(view.GetCursorEntity(world), grandchild);
	// grandchild is a leaf — 'h' walks to parent (child)
	EXPECT_EQ(view.HandleTextInput("h", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), child);
	// child is expanded and has children — 'h' collapses child
	EXPECT_EQ(view.HandleTextInput("h", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), child);
	EXPECT_EQ(view.GetCursorRow(), 1u);
}

TEST(HierarchyView, test_hierarchy_text_input_l_expands_or_walks_to_first_child)
{
	ecs::World world;
	const ecs::Entity root = world.CreateEntity("Root");
	const ecs::Entity childA = world.CreateEntity("ChildA");
	const ecs::Entity childB = world.CreateEntity("ChildB");
	ASSERT_EQ(world.SetParent(childA, root), ecs::EcsError::None);
	ASSERT_EQ(world.SetParent(childB, root), ecs::EcsError::None);

	editor::HierarchyView view;

	EXPECT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), root);
	EXPECT_EQ(view.GetCursorRow(), 0u);

	EXPECT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), childA);
	EXPECT_EQ(view.GetCursorRow(), 1u);

	EXPECT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorEntity(world), childA);
	EXPECT_EQ(view.GetCursorRow(), 1u);
}

TEST(HierarchyView, test_hierarchy_text_input_j_moves_cursor_down_one_visible_row)
{
	ecs::World world;
	const ecs::Entity root = world.CreateEntity("Root");
	const ecs::Entity child = world.CreateEntity("Child");
	ASSERT_EQ(world.SetParent(child, root), ecs::EcsError::None);

	editor::HierarchyView view;
	ASSERT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);

	EXPECT_EQ(view.GetCursorRow(), 0u);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 1u);
	EXPECT_EQ(view.GetCursorEntity(world), child);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 1u);
	EXPECT_EQ(view.GetCursorEntity(world), child);
}

TEST(HierarchyView, test_hierarchy_text_input_k_moves_cursor_up_one_visible_row)
{
	ecs::World world;
	const ecs::Entity root = world.CreateEntity("Root");
	const ecs::Entity child = world.CreateEntity("Child");
	ASSERT_EQ(world.SetParent(child, root), ecs::EcsError::None);

	editor::HierarchyView view;
	ASSERT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	ASSERT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	ASSERT_EQ(view.GetCursorRow(), 1u);

	EXPECT_EQ(view.HandleTextInput("k", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 0u);
	EXPECT_EQ(view.GetCursorEntity(world), root);
	EXPECT_EQ(view.HandleTextInput("k", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 0u);
	EXPECT_EQ(view.GetCursorEntity(world), root);
}

TEST(HierarchyView, test_hierarchy_text_input_gg_moves_cursor_to_first_visible_row)
{
	ecs::World world;
	const ecs::Entity root = world.CreateEntity("Root");
	const ecs::Entity child = world.CreateEntity("Child");
	ASSERT_EQ(world.SetParent(child, root), ecs::EcsError::None);

	editor::HierarchyView view;
	ASSERT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	ASSERT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	ASSERT_EQ(view.GetCursorRow(), 1u);

	EXPECT_EQ(view.HandleTextInput("g", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 1u);
	EXPECT_EQ(view.HandleTextInput("g", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 0u);
	EXPECT_EQ(view.GetCursorEntity(world), root);

	EXPECT_EQ(view.HandleTextInput("g", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 1u);
	EXPECT_EQ(view.GetCursorEntity(world), child);
}

TEST(HierarchyView, test_hierarchy_text_input_capital_G_moves_cursor_to_last_visible_row)
{
	ecs::World emptyWorld;
	editor::HierarchyView emptyView;
	EXPECT_EQ(emptyView.HandleTextInput("G", emptyWorld), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(emptyView.GetCursorRow(), 0u);

	ecs::World world;
	const ecs::Entity root = world.CreateEntity("Root");
	const ecs::Entity child = world.CreateEntity("Child");
	ASSERT_EQ(world.SetParent(child, root), ecs::EcsError::None);

	editor::HierarchyView view;
	ASSERT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.HandleTextInput("G", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.GetCursorRow(), 1u);
	EXPECT_EQ(view.GetCursorEntity(world), child);
}

TEST(HierarchyView, test_hierarchy_text_input_colon_requests_command_bar)
{
	ecs::World world;
	const ecs::Entity root = world.CreateEntity("Root");
	const ecs::Entity child = world.CreateEntity("Child");
	ASSERT_EQ(world.SetParent(child, root), ecs::EcsError::None);

	editor::HierarchyView view;
	ASSERT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	ASSERT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	ASSERT_EQ(view.GetCursorEntity(world), child);
	ASSERT_EQ(view.GetCursorRow(), 1u);

	EXPECT_EQ(view.HandleTextInput("g", world), editor::HierarchyViewKeyResult::Consumed);
	EXPECT_EQ(view.HandleTextInput(":j", world), editor::HierarchyViewKeyResult::RequestCommandBar);
	EXPECT_EQ(view.GetCursorRow(), 1u);
	EXPECT_EQ(view.GetCursorEntity(world), child);
}

TEST(HierarchyView, test_render_clamps_cursor_then_delegates_to_panel)
{
	ecs::World world;
	const ecs::Entity root = world.CreateEntity("Root");
	const ecs::Entity child = world.CreateEntity("Child");
	ASSERT_EQ(world.SetParent(child, root), ecs::EcsError::None);

	editor::HierarchyView view;
	ASSERT_EQ(view.HandleTextInput("l", world), editor::HierarchyViewKeyResult::Consumed);
	ASSERT_EQ(view.HandleTextInput("j", world), editor::HierarchyViewKeyResult::Consumed);
	ASSERT_EQ(view.GetCursorRow(), 1u);
	world.DestroyEntity(child);
	ASSERT_EQ(view.GetCursorRow(), 1u);

	ui::DrawList drawList;
	drawList.AddQuad(ui::QuadCommand{1.0f, 2.0f, 3.0f, 4.0f, {0.1f, 0.2f, 0.3f, 0.4f}});

	ui::FontAtlas atlas;
	view.Render(drawList, world, atlas, 10.0f, 20.0f, 200.0f, 100.0f);

	EXPECT_EQ(view.GetCursorRow(), 0u);

	const auto quads = drawList.GetQuads();
	const auto texts = drawList.GetTexts();
	EXPECT_EQ(quads.size(), 3u);
	EXPECT_EQ(texts.size(), 1u);
	EXPECT_EQ(drawList.GetImageQuads().size(), 0u);

	const ui::HierarchyPanelConfig& config = view.GetPanel().GetConfig();
	const ui::QuadCommand& background = quads[1];
	EXPECT_FLOAT_EQ(background.x, 10.0f);
	EXPECT_FLOAT_EQ(background.y, 20.0f);
	EXPECT_FLOAT_EQ(background.width, 200.0f);
	EXPECT_FLOAT_EQ(background.height, 100.0f);
	EXPECT_FLOAT_EQ(background.color.r, config.background.r);
	EXPECT_FLOAT_EQ(background.color.g, config.background.g);
	EXPECT_FLOAT_EQ(background.color.b, config.background.b);
	EXPECT_FLOAT_EQ(background.color.a, config.background.a);

	const ui::QuadCommand& highlight = quads[2];
	const float rowHeight = atlas.GetAscender() - atlas.GetDescender();
	EXPECT_FLOAT_EQ(highlight.x, 10.0f);
	EXPECT_FLOAT_EQ(highlight.y, 20.0f + config.paddingY);
	EXPECT_FLOAT_EQ(highlight.width, 200.0f);
	EXPECT_FLOAT_EQ(highlight.height, rowHeight);
	EXPECT_FLOAT_EQ(highlight.color.r, config.cursorRowBackground.r);
	EXPECT_FLOAT_EQ(highlight.color.g, config.cursorRowBackground.g);
	EXPECT_FLOAT_EQ(highlight.color.b, config.cursorRowBackground.b);
	EXPECT_FLOAT_EQ(highlight.color.a, config.cursorRowBackground.a);

	const ui::TextCommand& rootText = texts[0];
	const std::string_view buffer = drawList.GetTextBuffer();
	EXPECT_EQ(buffer.substr(rootText.textOffset, rootText.textLength), "Root");
	EXPECT_FLOAT_EQ(rootText.x, 10.0f + config.paddingX);
	EXPECT_FLOAT_EQ(rootText.y, 20.0f + config.paddingY + atlas.GetAscender());
	EXPECT_FLOAT_EQ(rootText.color.r, config.foreground.r);
	EXPECT_FLOAT_EQ(rootText.color.g, config.foreground.g);
	EXPECT_FLOAT_EQ(rootText.color.b, config.foreground.b);
	EXPECT_FLOAT_EQ(rootText.color.a, config.foreground.a);
}
