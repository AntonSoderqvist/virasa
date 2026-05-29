#include <gtest/gtest.h>

#include "virasa/editor/ViewManager.h"
#include "virasa/editor/CommandBarView.h"
#include "virasa/editor/EditorView.h"
#include "virasa/editor/HierarchyView.h"
#include "virasa/editor/EntityEditorView.h"
#include "virasa/editor/Motions.h"
#include "virasa/ecs/World.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/Types.h"
#include "virasa/window/Events.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

using namespace virasa::editor;

// ---------------------------------------------------------------------------
// Helper: build a KeyDown event for a given KeyCode
// ---------------------------------------------------------------------------
namespace
{

virasa::Event MakeKeyEvent(virasa::KeyCode key)
{
	virasa::Event ev;
	ev.type = virasa::EventType::KeyDown;
	ev.keyboard.key = key;
	ev.keyboard.repeat = false;
	return ev;
}

virasa::Event MakeTextEvent(const char* utf8Text)
{
	virasa::Event ev;
	ev.type = virasa::EventType::TextInput;
	ev.textInput = {};
	uint8_t len = 0;
	while (utf8Text[len] != '\0' && len < 31)
	{
		ev.textInput.utf8[len] = utf8Text[len];
		++len;
	}
	ev.textInput.utf8[len] = '\0';
	ev.textInput.length = len;
	return ev;
}

virasa::Event MakeUnknownEvent()
{
	virasa::Event ev;
	ev.type = virasa::EventType::Quit;
	return ev;
}

std::string GetTextCommandText(const virasa::ui::DrawList& drawList, std::size_t index)
{
	const auto texts = drawList.GetTexts();
	const auto buffer = drawList.GetTextBuffer();
	const auto& command = texts[index];
	return std::string(buffer.substr(command.textOffset, command.textLength));
}

} // namespace

// ===========================================================================
// view_manager_owns_three_views_focus_and_right_panel_mode
// ===========================================================================
TEST(ViewManager, test_view_manager_owns_three_views_focus_and_right_panel_mode)
{
	static_assert(std::is_final_v<ViewManager>);
	static_assert(std::is_default_constructible_v<ViewManager>);
	static_assert(std::is_move_constructible_v<ViewManager>);
	static_assert(std::is_move_assignable_v<ViewManager>);
	static_assert(!std::is_copy_constructible_v<ViewManager>);
	static_assert(!std::is_copy_assignable_v<ViewManager>);

	ViewManager vm;

	EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
	EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);

	[[maybe_unused]] CommandBarView& cbv = vm.GetCommandBarView();
	[[maybe_unused]] EditorView& ev = vm.GetEditorView();
	[[maybe_unused]] HierarchyView& hv = vm.GetHierarchyView();
	[[maybe_unused]] EntityEditorView& eev = vm.GetEntityEditorView();

	const ViewManager& cvm = vm;
	[[maybe_unused]] const CommandBarView& ccbv = cvm.GetCommandBarView();
	[[maybe_unused]] const EditorView& cev = cvm.GetEditorView();
	[[maybe_unused]] const HierarchyView& chv = cvm.GetHierarchyView();
	[[maybe_unused]] const EntityEditorView& ceev = cvm.GetEntityEditorView();

	ViewManager moved = std::move(vm);
	EXPECT_EQ(moved.GetFocus(), Focus::CommandBar);
	EXPECT_EQ(moved.GetRightPanelMode(), RightPanelMode::Closed);
}

// ===========================================================================
// focus_enum_values_in_declared_order
// ===========================================================================
TEST(ViewManager, test_focus_enum_values_in_declared_order)
{
	static_assert(std::is_same_v<std::underlying_type_t<Focus>, uint8_t>);

	EXPECT_EQ(static_cast<uint8_t>(Focus::CommandBar), uint8_t{0});
	EXPECT_EQ(static_cast<uint8_t>(Focus::Editor), uint8_t{1});
	EXPECT_EQ(static_cast<uint8_t>(Focus::Hierarchy), uint8_t{2});
	EXPECT_EQ(static_cast<uint8_t>(Focus::EntityEditor), uint8_t{3});

	ViewManager vm;
	EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
}

// ===========================================================================
// right_panel_mode_enum_values_in_declared_order
// ===========================================================================
TEST(ViewManager, test_right_panel_mode_enum_values_in_declared_order)
{
	static_assert(std::is_same_v<std::underlying_type_t<RightPanelMode>, uint8_t>);

	EXPECT_EQ(static_cast<uint8_t>(RightPanelMode::Closed), uint8_t{0});
	EXPECT_EQ(static_cast<uint8_t>(RightPanelMode::Editor), uint8_t{1});
	EXPECT_EQ(static_cast<uint8_t>(RightPanelMode::Hierarchy), uint8_t{2});

	ViewManager vm;
	EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
}

// ===========================================================================
// event_result_enum_values_in_declared_order
// ===========================================================================
TEST(ViewManager, test_event_result_enum_values_in_declared_order)
{
	static_assert(std::is_same_v<std::underlying_type_t<EventResult>, uint8_t>);

	EXPECT_EQ(static_cast<uint8_t>(EventResult::Consumed), uint8_t{0});
	EXPECT_EQ(static_cast<uint8_t>(EventResult::QuitRequested), uint8_t{1});
	EXPECT_EQ(static_cast<uint8_t>(EventResult::LoadModelRequested), uint8_t{2});
}

// ===========================================================================
// get_focus_returns_current_focus
// ===========================================================================
TEST(ViewManager, test_get_focus_returns_current_focus)
{
	const ViewManager vm;
	// Default is CommandBar
	EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);

	// GetFocus is const and noexcept — verified via const ref above.
	// We also verify the return type is Focus.
	static_assert(std::is_same_v<decltype(vm.GetFocus()), Focus>);
}

// ===========================================================================
// get_right_panel_mode_returns_current_mode
// ===========================================================================
TEST(ViewManager, test_get_right_panel_mode_returns_current_mode)
{
	const ViewManager vm;
	EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);

	static_assert(std::is_same_v<decltype(vm.GetRightPanelMode()), RightPanelMode>);
}

// ===========================================================================
// get_pending_load_path_returns_last_load_request
// ===========================================================================
TEST(ViewManager, test_get_pending_load_path_returns_last_load_request)
{
	virasa::ecs::World world;
	ViewManager vm;

	EXPECT_TRUE(vm.GetPendingLoadPath().empty());

	vm.GetCommandBarView().SetText(":load first.glb");
	EXPECT_EQ(vm.HandleEvent(MakeKeyEvent(virasa::KeyCode::Enter), world),
		EventResult::LoadModelRequested);
	EXPECT_EQ(vm.GetPendingLoadPath(), "first.glb");

	const std::string_view firstView = vm.GetPendingLoadPath();
	EXPECT_EQ(vm.HandleEvent(MakeUnknownEvent(), world), EventResult::Consumed);
	EXPECT_EQ(vm.GetPendingLoadPath(), "first.glb");
	EXPECT_EQ(firstView, "first.glb");

	virasa::ui::DrawList drawList;
	virasa::ui::FontAtlas atlas;
	vm.Render(drawList, world, atlas, 320u, 200u);
	EXPECT_EQ(vm.GetPendingLoadPath(), "first.glb");
	EXPECT_EQ(firstView, "first.glb");

	vm.GetCommandBarView().SetText(":load second.glb");
	EXPECT_EQ(vm.HandleEvent(MakeKeyEvent(virasa::KeyCode::Enter), world),
		EventResult::LoadModelRequested);
	EXPECT_EQ(vm.GetPendingLoadPath(), "second.glb");
}

// ===========================================================================
// get_view_accessors_return_owned_views
// ===========================================================================
TEST(ViewManager, test_get_view_accessors_return_owned_views)
{
	ViewManager vm;

	CommandBarView& cbvRef = vm.GetCommandBarView();
	EditorView& evRef = vm.GetEditorView();
	HierarchyView& hvRef = vm.GetHierarchyView();
	EntityEditorView& eevRef = vm.GetEntityEditorView();

	EXPECT_EQ(&vm.GetCommandBarView(), &cbvRef);
	EXPECT_EQ(&vm.GetEditorView(), &evRef);
	EXPECT_EQ(&vm.GetHierarchyView(), &hvRef);
	EXPECT_EQ(&vm.GetEntityEditorView(), &eevRef);

	const ViewManager& cvm = vm;
	const CommandBarView& ccbvRef = cvm.GetCommandBarView();
	const EditorView& cevRef = cvm.GetEditorView();
	const HierarchyView& chvRef = cvm.GetHierarchyView();
	const EntityEditorView& ceevRef = cvm.GetEntityEditorView();

	EXPECT_EQ(&ccbvRef, &cbvRef);
	EXPECT_EQ(&cevRef, &evRef);
	EXPECT_EQ(&chvRef, &hvRef);
	EXPECT_EQ(&ceevRef, &eevRef);

	EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
	EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
}

// ===========================================================================
// handle_event_forwards_to_focused_view
// ===========================================================================
TEST(ViewManager, test_handle_event_forwards_to_focused_view)
{
	virasa::ecs::World world;
	const virasa::Event enterEv = MakeKeyEvent(virasa::KeyCode::Enter);

	{
		ViewManager vm;
		const virasa::Event textEv = MakeTextEvent("abc");
		EXPECT_EQ(vm.HandleEvent(textEv, world), EventResult::Consumed);
		EXPECT_EQ(vm.GetCommandBarView().GetText(), "abc");
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
	}

	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":ide");
		ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		ASSERT_EQ(vm.GetFocus(), Focus::Editor);
		ASSERT_EQ(vm.GetRightPanelMode(), RightPanelMode::Editor);

		EXPECT_EQ(vm.HandleEvent(MakeTextEvent("i"), world), EventResult::Consumed);
		EXPECT_EQ(vm.GetEditorView().GetMotionState().GetMode(), virasa::editor::Mode::Insert);
		EXPECT_EQ(vm.HandleEvent(MakeTextEvent("Z"), world), EventResult::Consumed);
		EXPECT_EQ(vm.GetEditorView().GetBuffer().GetText(), "Z");
	}

	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":tree");
		ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		ASSERT_EQ(vm.GetFocus(), Focus::Hierarchy);
		ASSERT_EQ(vm.GetRightPanelMode(), RightPanelMode::Hierarchy);

		const virasa::ecs::Entity entity = world.CreateEntity("EntityA");
		(void)entity;
		EXPECT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		EXPECT_EQ(vm.GetFocus(), Focus::EntityEditor);
	}

	{
		ViewManager vm;
		const virasa::ecs::Entity entity = world.CreateEntity("EntityB");
		(void)entity;
		vm.GetCommandBarView().SetText(":tree");
		ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		ASSERT_EQ(vm.GetFocus(), Focus::EntityEditor);

		EXPECT_EQ(vm.HandleEvent(MakeKeyEvent(virasa::KeyCode::Escape), world),
			EventResult::Consumed);
		EXPECT_EQ(vm.GetFocus(), Focus::Hierarchy);
	}

	{
		ViewManager vm;
		EXPECT_EQ(vm.HandleEvent(MakeUnknownEvent(), world), EventResult::Consumed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
	}
}

// ===========================================================================
// handle_event_consumes_request_command_bar_results
// ===========================================================================
TEST(ViewManager, test_handle_event_consumes_request_command_bar_results)
{
	virasa::ecs::World world;
	const virasa::Event enterEv = MakeKeyEvent(virasa::KeyCode::Enter);

	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":ide");
		ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		ASSERT_EQ(vm.GetFocus(), Focus::Editor);

		EXPECT_EQ(vm.HandleEvent(MakeTextEvent(":"), world), EventResult::Consumed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
		EXPECT_EQ(vm.GetCommandBarView().GetText(), ":");
		EXPECT_EQ(vm.GetCommandBarView().GetCursorByte(), std::size_t{1});
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Editor);
	}

	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":tree");
		ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		ASSERT_EQ(vm.GetFocus(), Focus::Hierarchy);

		EXPECT_EQ(vm.HandleEvent(MakeTextEvent(":"), world), EventResult::Consumed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
		EXPECT_EQ(vm.GetCommandBarView().GetText(), ":");
		EXPECT_EQ(vm.GetCommandBarView().GetCursorByte(), std::size_t{1});
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Hierarchy);
	}

	{
		ViewManager vm;
		const virasa::ecs::Entity entity = world.CreateEntity("EntityForCommandBar");
		(void)entity;
		vm.GetCommandBarView().SetText(":tree");
		ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		ASSERT_EQ(vm.GetFocus(), Focus::EntityEditor);
		ASSERT_EQ(vm.GetRightPanelMode(), RightPanelMode::Hierarchy);

		EXPECT_EQ(vm.HandleEvent(MakeTextEvent(":"), world), EventResult::Consumed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
		EXPECT_EQ(vm.GetCommandBarView().GetText(), ":");
		EXPECT_EQ(vm.GetCommandBarView().GetCursorByte(), std::size_t{1});
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Hierarchy);
	}
}

// ===========================================================================
// handle_event_consumes_request_entity_editor_result
// ===========================================================================
TEST(ViewManager, test_handle_event_consumes_request_entity_editor_result)
{
	virasa::ecs::World world;
	const virasa::ecs::Entity entity = world.CreateEntity("EntityForEditor");
	(void)entity;
	const virasa::Event enterEv = MakeKeyEvent(virasa::KeyCode::Enter);

	ViewManager vm;
	vm.GetCommandBarView().SetText(":tree");
	ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
	ASSERT_EQ(vm.GetFocus(), Focus::Hierarchy);
	ASSERT_EQ(vm.GetRightPanelMode(), RightPanelMode::Hierarchy);
	const std::size_t cursorRowBefore = vm.GetHierarchyView().GetCursorRow();

	EXPECT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
	EXPECT_EQ(vm.GetFocus(), Focus::EntityEditor);
	EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Hierarchy);
	EXPECT_EQ(vm.GetHierarchyView().GetCursorRow(), cursorRowBefore);
	EXPECT_TRUE(vm.GetCommandBarView().GetText().empty());
}

// ===========================================================================
// handle_event_consumes_request_hierarchy_result
// ===========================================================================
TEST(ViewManager, test_handle_event_consumes_request_hierarchy_result)
{
	virasa::ecs::World world;
	const virasa::ecs::Entity entity = world.CreateEntity("EntityForHierarchy");
	(void)entity;
	const virasa::Event enterEv = MakeKeyEvent(virasa::KeyCode::Enter);

	ViewManager vm;
	vm.GetCommandBarView().SetText(":tree");
	ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
	ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
	ASSERT_EQ(vm.GetFocus(), Focus::EntityEditor);
	ASSERT_EQ(vm.GetRightPanelMode(), RightPanelMode::Hierarchy);
	const std::size_t hierarchyCursorBefore = vm.GetHierarchyView().GetCursorRow();
	const std::size_t entityEditorRowBefore = vm.GetEntityEditorView().GetCursorRow();
	const std::size_t entityEditorCellBefore = vm.GetEntityEditorView().GetCursorCell();

	EXPECT_EQ(vm.HandleEvent(MakeKeyEvent(virasa::KeyCode::Escape), world),
		EventResult::Consumed);
	EXPECT_EQ(vm.GetFocus(), Focus::Hierarchy);
	EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Hierarchy);
	EXPECT_EQ(vm.GetHierarchyView().GetCursorRow(), hierarchyCursorBefore);
	EXPECT_EQ(vm.GetEntityEditorView().GetCursorRow(), entityEditorRowBefore);
	EXPECT_EQ(vm.GetEntityEditorView().GetCursorCell(), entityEditorCellBefore);
	EXPECT_TRUE(vm.GetCommandBarView().GetText().empty());
}

// ===========================================================================
// handle_event_consumes_command_bar_submitted_results
// ===========================================================================
TEST(ViewManager, test_handle_event_consumes_command_bar_submitted_results)
{
	virasa::ecs::World world;
	const virasa::Event enterEv = MakeKeyEvent(virasa::KeyCode::Enter);

	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":unknown");
		EXPECT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
		EXPECT_TRUE(vm.GetCommandBarView().GetText().empty());
		EXPECT_TRUE(vm.GetPendingLoadPath().empty());
	}

	{
		ViewManager vm;
		vm.GetEditorView().GetMotionState().SetMode(virasa::editor::Mode::Insert);
		vm.GetCommandBarView().SetText(":ide");
		EXPECT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Editor);
		EXPECT_EQ(vm.GetFocus(), Focus::Editor);
		EXPECT_EQ(vm.GetEditorView().GetMotionState().GetMode(), virasa::editor::Mode::Normal);
	}

	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":ide");
		ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		ASSERT_EQ(vm.GetRightPanelMode(), RightPanelMode::Editor);
		ASSERT_EQ(vm.HandleEvent(MakeTextEvent(":"), world), EventResult::Consumed);
		ASSERT_EQ(vm.GetFocus(), Focus::CommandBar);
		vm.GetCommandBarView().SetText(":ide");
		EXPECT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
	}

	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":tree");
		EXPECT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Hierarchy);
		EXPECT_EQ(vm.GetFocus(), Focus::Hierarchy);
	}

	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":tree");
		ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		ASSERT_EQ(vm.GetRightPanelMode(), RightPanelMode::Hierarchy);
		ASSERT_EQ(vm.HandleEvent(MakeTextEvent(":"), world), EventResult::Consumed);
		ASSERT_EQ(vm.GetFocus(), Focus::CommandBar);
		vm.GetCommandBarView().SetText(":tree");
		EXPECT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
	}

	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":ide");
		ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		ASSERT_EQ(vm.HandleEvent(MakeTextEvent(":"), world), EventResult::Consumed);
		ASSERT_EQ(vm.GetFocus(), Focus::CommandBar);
		vm.GetCommandBarView().SetText(":");
		EXPECT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
	}

	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":q");
		EXPECT_EQ(vm.HandleEvent(enterEv, world), EventResult::QuitRequested);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
	}

	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":ide");
		ASSERT_EQ(vm.HandleEvent(enterEv, world), EventResult::Consumed);
		ASSERT_EQ(vm.GetRightPanelMode(), RightPanelMode::Editor);
		ASSERT_EQ(vm.GetFocus(), Focus::Editor);
		vm.GetCommandBarView().SetText(":load model_a.glb");
		EXPECT_EQ(vm.HandleEvent(enterEv, world), EventResult::LoadModelRequested);
		EXPECT_EQ(vm.GetPendingLoadPath(), "model_a.glb");
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Editor);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
	}

	{
		ViewManager vm;
		vm.GetCommandBarView().SetText("seed");
		EXPECT_EQ(vm.HandleEvent(MakeTextEvent("x"), world), EventResult::Consumed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
		EXPECT_EQ(vm.GetCommandBarView().GetText(), "seedx");
	}
}


// ===========================================================================
// render_lays_out_views_and_delegates
// ===========================================================================
TEST(ViewManager, test_render_lays_out_views_and_delegates)
{
	virasa::ecs::World world;
	const virasa::ecs::Entity entity = world.CreateEntity("RenderEntity");
	(void)entity;
	virasa::ui::FontAtlas atlas;
	virasa::ui::DrawList drawList;

	const uint32_t kWidth = 800u;
	const uint32_t kHeight = 600u;

	ViewManager vm;
	const float barHeight = atlas.GetAscender() - atlas.GetDescender() +
		2.0f * vm.GetCommandBarView().GetPanel().GetConfig().paddingY;
	const float expectedPanelHeight = std::max(0.0f, static_cast<float>(kHeight) - barHeight);
	const float expectedHierarchyHeight = std::floor(expectedPanelHeight * 0.5f);
	const float expectedEntityEditorY = expectedHierarchyHeight;
	const float expectedEntityEditorHeight = expectedPanelHeight - expectedHierarchyHeight;

	vm.Render(drawList, world, atlas, kWidth, kHeight);
	EXPECT_EQ(drawList.GetImageQuads().size(), std::size_t{0});
	ASSERT_GE(drawList.GetQuads().size(), std::size_t{1});
	EXPECT_EQ(drawList.GetQuads()[0].x, 0.0f);
	EXPECT_EQ(drawList.GetQuads()[0].y, static_cast<float>(kHeight) - barHeight);
	EXPECT_EQ(drawList.GetQuads()[0].width, static_cast<float>(kWidth));
	EXPECT_EQ(drawList.GetQuads()[0].height, barHeight);
	const std::size_t closedQuadCount = drawList.GetQuads().size();
	drawList.Clear();

	vm.GetCommandBarView().SetText(":ide");
	ASSERT_EQ(vm.HandleEvent(MakeKeyEvent(virasa::KeyCode::Enter), world), EventResult::Consumed);
	vm.Render(drawList, world, atlas, kWidth, kHeight);
	EXPECT_EQ(drawList.GetImageQuads().size(), std::size_t{0});
	EXPECT_GT(drawList.GetQuads().size(), closedQuadCount);
	ASSERT_GE(drawList.GetQuads().size(), std::size_t{2});
	EXPECT_EQ(drawList.GetQuads()[0].x, static_cast<float>(kWidth / 2u));
	EXPECT_EQ(drawList.GetQuads()[0].y, 0.0f);
	EXPECT_EQ(drawList.GetQuads()[0].width, static_cast<float>(kWidth - (kWidth / 2u)));
	EXPECT_EQ(drawList.GetQuads()[0].height, expectedPanelHeight);
	EXPECT_EQ(drawList.GetQuads().back().x, 0.0f);
	EXPECT_EQ(drawList.GetQuads().back().y, static_cast<float>(kHeight) - barHeight);
	drawList.Clear();

	vm.HandleEvent(MakeTextEvent(":"), world);
	vm.GetCommandBarView().SetText(":tree");
	ASSERT_EQ(vm.HandleEvent(MakeKeyEvent(virasa::KeyCode::Enter), world), EventResult::Consumed);
	vm.Render(drawList, world, atlas, kWidth, kHeight);
	EXPECT_EQ(drawList.GetImageQuads().size(), std::size_t{0});
	ASSERT_GE(drawList.GetQuads().size(), std::size_t{3});
	EXPECT_EQ(drawList.GetQuads()[0].x, static_cast<float>(kWidth / 2u));
	EXPECT_EQ(drawList.GetQuads()[0].y, 0.0f);
	EXPECT_EQ(drawList.GetQuads()[0].width, static_cast<float>(kWidth - (kWidth / 2u)));
	EXPECT_EQ(drawList.GetQuads()[0].height, expectedHierarchyHeight);
	EXPECT_EQ(drawList.GetQuads()[1].x, static_cast<float>(kWidth / 2u));
	EXPECT_EQ(drawList.GetQuads()[1].y, expectedEntityEditorY);
	EXPECT_EQ(drawList.GetQuads()[1].width, static_cast<float>(kWidth - (kWidth / 2u)));
	EXPECT_EQ(drawList.GetQuads()[1].height, expectedEntityEditorHeight);
	EXPECT_EQ(drawList.GetQuads().back().x, 0.0f);
	EXPECT_EQ(drawList.GetQuads().back().y, static_cast<float>(kHeight) - barHeight);

	bool foundEntityName = false;
	for (std::size_t i = 0; i < drawList.GetTexts().size(); ++i)
	{
		if (GetTextCommandText(drawList, i) == "RenderEntity")
		{
			foundEntityName = true;
			break;
		}
	}
	EXPECT_TRUE(foundEntityName);
}
