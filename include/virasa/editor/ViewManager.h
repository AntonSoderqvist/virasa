#ifndef VIRASA_EDITOR_VIEWMANAGER_H
#define VIRASA_EDITOR_VIEWMANAGER_H

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "virasa/ecs/Types.h"
#include "virasa/editor/CommandBarView.h"
#include "virasa/editor/EditorView.h"
#include "virasa/editor/EntityEditorView.h"
#include "virasa/editor/HierarchyView.h"
#include "virasa/ui/Types.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ecs/World.h"
#include "virasa/window/Events.h"

namespace virasa::editor
{

/**
 * @brief Enumerates which view currently receives input focus.
 */
enum class Focus : uint8_t
{
	CommandBar,
	Editor,
	Hierarchy,
	EntityEditor
};

/**
 * @brief Enumerates which view (if any) is rendered in the right panel.
 */
enum class RightPanelMode : uint8_t
{
	Closed,
	Editor,
	Hierarchy
};

/**
 * @brief Result returned by ViewManager::HandleEvent.
 */
enum class EventResult : uint8_t
{
	Consumed,
	QuitRequested,
	LoadModelRequested,
	PlayRequested,
	StopRequested,
	LoadSceneRequested,
	SaveSceneRequested
};

/**
 * @brief Owns and coordinates the editor views, input focus, panel mode, and selection.
 *
 * ViewManager is the top-level coordinator for the editor UI. It owns a CommandBarView,
 * an EditorView, a HierarchyView, and an EntityEditorView, tracks which view has input
 * focus, determines which view is rendered in the right panel, stores the most recent
 * pending model-load path, and owns the committed entity selection. It dispatches input
 * events to the focused view and translates view-level results into application-level
 * EventResult values. ViewManager performs no Vulkan API calls and owns no Vulkan resources.
 */
class ViewManager final
{
public:
	ViewManager() = default;
	ViewManager(const ViewManager&) = delete;
	ViewManager& operator=(const ViewManager&) = delete;
	ViewManager(ViewManager&&) noexcept = default;
	ViewManager& operator=(ViewManager&&) noexcept = default;
	~ViewManager() = default;

	/**
	 * @brief Returns the current input focus.
	 * @return The Focus enum value naming the currently focused view.
	 */
	[[nodiscard]] Focus GetFocus() const noexcept;

	/**
	 * @brief Returns the current right-panel mode.
	 * @return The RightPanelMode enum value naming the active right-panel state.
	 */
	[[nodiscard]] RightPanelMode GetRightPanelMode() const noexcept;

	/**
	 * @brief Returns a mutable reference to the owned CommandBarView.
	 * @return Mutable reference to _commandBarView.
	 */
	[[nodiscard]] virasa::editor::CommandBarView& GetCommandBarView() noexcept;

	/**
	 * @brief Returns a const reference to the owned CommandBarView.
	 * @return Const reference to _commandBarView.
	 */
	[[nodiscard]] const virasa::editor::CommandBarView& GetCommandBarView() const noexcept;

	/**
	 * @brief Returns a mutable reference to the owned EditorView.
	 * @return Mutable reference to _editorView.
	 */
	[[nodiscard]] virasa::editor::EditorView& GetEditorView() noexcept;

	/**
	 * @brief Returns a const reference to the owned EditorView.
	 * @return Const reference to _editorView.
	 */
	[[nodiscard]] const virasa::editor::EditorView& GetEditorView() const noexcept;

	/**
	 * @brief Returns a mutable reference to the owned HierarchyView.
	 * @return Mutable reference to _hierarchyView.
	 */
	[[nodiscard]] virasa::editor::HierarchyView& GetHierarchyView() noexcept;

	/**
	 * @brief Returns a const reference to the owned HierarchyView.
	 * @return Const reference to _hierarchyView.
	 */
	[[nodiscard]] const virasa::editor::HierarchyView& GetHierarchyView() const noexcept;

	/**
	 * @brief Returns a mutable reference to the owned EntityEditorView.
	 * @return Mutable reference to _entityEditorView.
	 */
	[[nodiscard]] virasa::editor::EntityEditorView& GetEntityEditorView() noexcept;

	/**
	 * @brief Returns a const reference to the owned EntityEditorView.
	 * @return Const reference to _entityEditorView.
	 */
	[[nodiscard]] const virasa::editor::EntityEditorView& GetEntityEditorView() const noexcept;

	/**
	 * @brief Returns a string_view over the path of the most recently requested GLB load.
	 * @return A string_view over _pendingLoadPath; empty if no load has been requested.
	 */
	[[nodiscard]] std::string_view GetPendingLoadPath() const noexcept;

	/**
	 * @brief Returns a string_view over the path of the most recently requested scene save.
	 * @return A string_view over _pendingSavePath; empty if no save has been requested.
	 */
	[[nodiscard]] std::string_view GetPendingSavePath() const noexcept;

	/**
	 * @brief Makes the supplied entity the sole committed selection.
	 * @param entity The entity to select, or virasa::ecs::Entity::Invalid() to clear.
	 */
	void SetSelection(virasa::ecs::Entity entity);

	/**
	 * @brief Clears the committed selection.
	 */
	void ClearSelection();

	/**
	 * @brief Returns whether the supplied entity is currently selected.
	 * @param entity The entity to test.
	 * @return True if and only if the committed selection contains entity.
	 */
	[[nodiscard]] bool IsSelected(virasa::ecs::Entity entity) const noexcept;

	/**
	 * @brief Returns the active selected entity.
	 * @return The first selected entity, or virasa::ecs::Entity::Invalid() when none is selected.
	 */
	[[nodiscard]] virasa::ecs::Entity GetActiveSelection() const noexcept;

	/**
	 * @brief Returns a span over the committed selection.
	 * @return A span over _selection's contents.
	 */
	[[nodiscard]] std::span<const virasa::ecs::Entity> GetSelection() const noexcept;

	/**
	 * @brief Dispatches an input event to the focused view and processes the result.
	 * @param event The input event to handle.
	 * @param world The current ECS world (passed through to views that need it).
	 * @return EventResult::QuitRequested if ':q' was submitted, EventResult::LoadModelRequested or EventResult::LoadSceneRequested if ':load <path>' was submitted, EventResult::SaveSceneRequested if ':save <path>' was submitted, EventResult::PlayRequested if ':play' was submitted, EventResult::StopRequested if ':stop' was submitted, EventResult::Consumed otherwise.
	 */
	EventResult HandleEvent(const virasa::Event& event, const virasa::ecs::World& world);

	/**
	 * @brief Renders all visible views into the provided DrawList.
	 * @param out The DrawList to append draw commands into.
	 * @param world The current ECS world.
	 * @param atlas The font atlas used for text rendering.
	 * @param windowWidth The current window width in pixels.
	 * @param windowHeight The current window height in pixels.
	 */
	void Render(
		virasa::ui::DrawList& out,
		const virasa::ecs::World& world,
		const virasa::ui::FontAtlas& atlas,
		uint32_t windowWidth,
		uint32_t windowHeight
	);

private:
	[[nodiscard]] EventResult ApplyCommandResult(CommandResult cmd);

	virasa::editor::CommandBarView _commandBarView = {};
	virasa::editor::EditorView _editorView = {};
	virasa::editor::HierarchyView _hierarchyView = {};
	virasa::editor::EntityEditorView _entityEditorView = {};
	Focus _focus = Focus::CommandBar;
	RightPanelMode _rightPanelMode = RightPanelMode::Closed;
	std::string _pendingLoadPath = {};
	std::string _pendingSavePath = {};
	std::vector<virasa::ecs::Entity> _selection = {};
};

} // namespace virasa::editor

#endif // VIRASA_EDITOR_VIEWMANAGER_H
