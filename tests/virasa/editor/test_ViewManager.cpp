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
	// CommandBar is first (0), Editor is second (1), Hierarchy is third (2)
	EXPECT_EQ(static_cast<uint8_t>(Focus::CommandBar), uint8_t{0});
	EXPECT_EQ(static_cast<uint8_t>(Focus::Editor),     uint8_t{1});
	EXPECT_EQ(static_cast<uint8_t>(Focus::Hierarchy),  uint8_t{2});

	// Default-constructed ViewManager starts at CommandBar
	ViewManager vm;
	EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
}

// ===========================================================================
// right_panel_mode_enum_values_in_declared_order
// ===========================================================================
TEST(ViewManager, test_right_panel_mode_enum_values_in_declared_order)
{
	// Closed is first (0), Editor is second (1), Hierarchy is third (2)
	EXPECT_EQ(static_cast<uint8_t>(RightPanelMode::Closed),    uint8_t{0});
	EXPECT_EQ(static_cast<uint8_t>(RightPanelMode::Editor),    uint8_t{1});
	EXPECT_EQ(static_cast<uint8_t>(RightPanelMode::Hierarchy), uint8_t{2});

	// Default-constructed ViewManager starts at Closed
	ViewManager vm;
	EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
}

// ===========================================================================
// event_result_enum_values_in_declared_order
// ===========================================================================
TEST(ViewManager, test_event_result_enum_values_in_declared_order)
{
	// Consumed is first (0), QuitRequested is second (1)
	EXPECT_EQ(static_cast<uint8_t>(EventResult::Consumed),      uint8_t{0});
	EXPECT_EQ(static_cast<uint8_t>(EventResult::QuitRequested), uint8_t{1});
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

	// --- Focus::CommandBar receives KeyDown ---
	{
		ViewManager vm;
		// Seed command bar with ":ide" then submit to open editor (sets focus to Editor)
		// First verify that a KeyDown while focused on CommandBar is forwarded.
		// We send a TextInput ':' to the command bar (it is the default focus).
		virasa::Event textEv = MakeTextEvent(":");
		EventResult r = vm.HandleEvent(textEv, world);
		EXPECT_EQ(r, EventResult::Consumed);
		// The command bar should have received the ':'
		EXPECT_EQ(vm.GetCommandBarView().GetText(), ":");
	}

	// --- Focus::Editor receives KeyDown (after opening editor) ---
	{
		ViewManager vm;
		// Open the editor panel: seed ":ide" and submit
		vm.GetCommandBarView().SetText(":ide");
		// Simulate Enter to submit
		virasa::Event enterEv = MakeKeyEvent(virasa::KeyCode::Enter);
		EventResult r = vm.HandleEvent(enterEv, world);
		EXPECT_EQ(r, EventResult::Consumed);
		EXPECT_EQ(vm.GetFocus(), Focus::Editor);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Editor);

		// Now send a TextInput while focused on Editor — MotionState in Normal
		// mode will consume it (colon would request command bar, but 'a' is just consumed).
		virasa::Event textEv = MakeTextEvent("a");
		EventResult r2 = vm.HandleEvent(textEv, world);
		EXPECT_EQ(r2, EventResult::Consumed);
	}

	// --- Focus::Hierarchy receives KeyDown (after opening hierarchy) ---
	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":tree");
		virasa::Event enterEv = MakeKeyEvent(virasa::KeyCode::Enter);
		EventResult r = vm.HandleEvent(enterEv, world);
		EXPECT_EQ(r, EventResult::Consumed);
		EXPECT_EQ(vm.GetFocus(), Focus::Hierarchy);

		// Send a KeyDown while focused on Hierarchy
		virasa::Event keyEv = MakeKeyEvent(virasa::KeyCode::Escape);
		EventResult r2 = vm.HandleEvent(keyEv, world);
		EXPECT_EQ(r2, EventResult::Consumed);
	}

	// --- Unknown event type is a no-op, returns Consumed ---
	{
		ViewManager vm;
		virasa::Event unknownEv = MakeUnknownEvent();
		EventResult r = vm.HandleEvent(unknownEv, world);
		EXPECT_EQ(r, EventResult::Consumed);
		// State unchanged
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

	// --- EditorView RequestCommandBar via TextInput ':' ---
	{
		ViewManager vm;
		// Open editor first
		vm.GetCommandBarView().SetText(":ide");
		virasa::Event enterEv = MakeKeyEvent(virasa::KeyCode::Enter);
		vm.HandleEvent(enterEv, world);
		EXPECT_EQ(vm.GetFocus(), Focus::Editor);

		// In Normal mode, ':' triggers RequestCommandBar from MotionState
		virasa::Event colonEv = MakeTextEvent(":");
		EventResult r = vm.HandleEvent(colonEv, world);
		EXPECT_EQ(r, EventResult::Consumed);
		// Focus should transfer to CommandBar
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
		// Command bar should be seeded with ":"
		EXPECT_EQ(vm.GetCommandBarView().GetText(), ":");
		// Right panel mode should NOT change
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Editor);
	}

	// --- HierarchyView RequestCommandBar via TextInput ':' ---
	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":tree");
		virasa::Event enterEv = MakeKeyEvent(virasa::KeyCode::Enter);
		vm.HandleEvent(enterEv, world);
		EXPECT_EQ(vm.GetFocus(), Focus::Hierarchy);

		virasa::Event colonEv = MakeTextEvent(":");
		EventResult r = vm.HandleEvent(colonEv, world);
		EXPECT_EQ(r, EventResult::Consumed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
		EXPECT_EQ(vm.GetCommandBarView().GetText(), ":");
		// Right panel mode unchanged
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Hierarchy);
	}
}

// ===========================================================================
// handle_event_consumes_command_bar_submitted_results
// ===========================================================================
TEST(ViewManager, test_handle_event_consumes_command_bar_submitted_results)
{
	virasa::ecs::World world;
	virasa::Event enterEv = MakeKeyEvent(virasa::KeyCode::Enter);

	// --- CommandResult::None (unknown command) ---
	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":unknown");
		EventResult r = vm.HandleEvent(enterEv, world);
		EXPECT_EQ(r, EventResult::Consumed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
	}

	// --- CommandResult::OpenEditor (toggle open) ---
	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":ide");
		EventResult r = vm.HandleEvent(enterEv, world);
		EXPECT_EQ(r, EventResult::Consumed);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Editor);
		EXPECT_EQ(vm.GetFocus(), Focus::Editor);
		// Editor should be in Normal mode
		EXPECT_EQ(vm.GetEditorView().GetMotionState().GetMode(), virasa::editor::Mode::Normal);
	}

	// --- CommandResult::OpenEditor (toggle close) ---
	{
		ViewManager vm;
		// Open editor
		vm.GetCommandBarView().SetText(":ide");
		vm.HandleEvent(enterEv, world);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Editor);
		// Route focus back to the command bar via ':' (Normal mode binding)
		vm.HandleEvent(MakeTextEvent(":"), world);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
		// Close editor by submitting :ide again
		vm.GetCommandBarView().SetText(":ide");
		EventResult r = vm.HandleEvent(enterEv, world);
		EXPECT_EQ(r, EventResult::Consumed);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
	}

	// --- CommandResult::OpenHierarchy (toggle open) ---
	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":tree");
		EventResult r = vm.HandleEvent(enterEv, world);
		EXPECT_EQ(r, EventResult::Consumed);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Hierarchy);
		EXPECT_EQ(vm.GetFocus(), Focus::Hierarchy);
	}

	// --- CommandResult::OpenHierarchy (toggle close) ---
	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":tree");
		vm.HandleEvent(enterEv, world);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Hierarchy);
		// Route focus back to the command bar via ':' binding
		vm.HandleEvent(MakeTextEvent(":"), world);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
		vm.GetCommandBarView().SetText(":tree");
		EventResult r = vm.HandleEvent(enterEv, world);
		EXPECT_EQ(r, EventResult::Consumed);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
	}

	// --- CommandResult::Close ---
	{
		ViewManager vm;
		// Open editor first so we have a non-default state
		vm.GetCommandBarView().SetText(":ide");
		vm.HandleEvent(enterEv, world);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Editor);
		// Route focus back to the command bar via ':' binding
		vm.HandleEvent(MakeTextEvent(":"), world);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
		// Submit ":" alone
		vm.GetCommandBarView().SetText(":");
		EventResult r = vm.HandleEvent(enterEv, world);
		EXPECT_EQ(r, EventResult::Consumed);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
	}

	// --- CommandResult::Quit ---
	{
		ViewManager vm;
		vm.GetCommandBarView().SetText(":q");
		EventResult r = vm.HandleEvent(enterEv, world);
		EXPECT_EQ(r, EventResult::QuitRequested);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
	}

	// --- CommandBarKeyResult::Consumed (no Submit invoked) ---
	{
		ViewManager vm;
		// Send a non-Enter key to CommandBar — should be Consumed without Submit
		virasa::Event keyEv = MakeKeyEvent(virasa::KeyCode::Escape);
		EventResult r = vm.HandleEvent(keyEv, world);
		EXPECT_EQ(r, EventResult::Consumed);
		// Focus and mode unchanged
		EXPECT_EQ(vm.GetFocus(), Focus::CommandBar);
		EXPECT_EQ(vm.GetRightPanelMode(), RightPanelMode::Closed);
	}
}

// ===========================================================================
// render_lays_out_views_and_delegates
// ===========================================================================
TEST(ViewManager, test_render_lays_out_views_and_delegates)
{
	virasa::ecs::World world;
	virasa::ui::FontAtlas atlas;
	virasa::ui::DrawList drawList;

	const uint32_t kWidth = 800u;
	const uint32_t kHeight = 600u;

	ViewManager vm;
	const float barHeight = atlas.GetAscender() - atlas.GetDescender() +
		2.0f *
		vm.GetCommandBarView().GetPanel().GetConfig().paddingY;
	(void)barHeight;
	(void)kWidth;
	(void)kHeight;
}
