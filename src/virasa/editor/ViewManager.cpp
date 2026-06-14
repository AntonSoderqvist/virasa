#include "virasa/editor/ViewManager.h"

#include <cctype>
#include <cmath>

#include "virasa/editor/Motions.h"

namespace virasa::editor
{
namespace
{

[[nodiscard]] bool EqualsAsciiCaseInsensitive(std::string_view lhs, std::string_view rhs)
{
	if (lhs.size() != rhs.size())
	{
		return false;
	}

	for (size_t i = 0; i < lhs.size(); ++i)
	{
		const auto left = static_cast<unsigned char>(lhs[i]);
		const auto right = static_cast<unsigned char>(rhs[i]);
		if (std::tolower(left) != std::tolower(right))
		{
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool IsSceneLoadPath(std::string_view path)
{
	const size_t dot = path.find_last_of('.');
	if (dot == std::string_view::npos)
	{
		return false;
	}

	const std::string_view extension = path.substr(dot + 1u);
	return EqualsAsciiCaseInsensitive(extension, "scene")
		|| EqualsAsciiCaseInsensitive(extension, "json");
}

} // namespace

Focus ViewManager::GetFocus() const noexcept
{
	return _focus;
}

RightPanelMode ViewManager::GetRightPanelMode() const noexcept
{
	return _rightPanelMode;
}

virasa::editor::CommandBarView& ViewManager::GetCommandBarView() noexcept
{
	return _commandBarView;
}

const virasa::editor::CommandBarView& ViewManager::GetCommandBarView() const noexcept
{
	return _commandBarView;
}

virasa::editor::EditorView& ViewManager::GetEditorView() noexcept
{
	return _editorView;
}

const virasa::editor::EditorView& ViewManager::GetEditorView() const noexcept
{
	return _editorView;
}

virasa::editor::HierarchyView& ViewManager::GetHierarchyView() noexcept
{
	return _hierarchyView;
}

const virasa::editor::HierarchyView& ViewManager::GetHierarchyView() const noexcept
{
	return _hierarchyView;
}

virasa::editor::EntityEditorView& ViewManager::GetEntityEditorView() noexcept
{
	return _entityEditorView;
}

const virasa::editor::EntityEditorView& ViewManager::GetEntityEditorView() const noexcept
{
	return _entityEditorView;
}

std::string_view ViewManager::GetPendingLoadPath() const noexcept
{
	return _pendingLoadPath;
}

std::string_view ViewManager::GetPendingSavePath() const noexcept
{
	return _pendingSavePath;
}

void ViewManager::SetSelection(virasa::ecs::Entity entity)
{
	_selection.clear();
	if (entity.IsValid())
	{
		_selection.push_back(entity);
	}
}

void ViewManager::ClearSelection()
{
	_selection.clear();
}

bool ViewManager::IsSelected(virasa::ecs::Entity entity) const noexcept
{
	for (const virasa::ecs::Entity selected : _selection)
	{
		if (selected == entity)
		{
			return true;
		}
	}

	return false;
}

virasa::ecs::Entity ViewManager::GetActiveSelection() const noexcept
{
	if (_selection.empty())
	{
		return virasa::ecs::Entity::Invalid();
	}

	return _selection.front();
}

std::span<const virasa::ecs::Entity> ViewManager::GetSelection() const noexcept
{
	return _selection;
}

EventResult ViewManager::HandleEvent(const virasa::Event& event, const virasa::ecs::World& world)
{
	if (event.type == virasa::EventType::KeyDown)
	{
		virasa::KeyCode key = event.keyboard.key;

		switch (_focus)
		{
			case Focus::CommandBar:
			{
				CommandBarKeyResult result = _commandBarView.HandleKey(key);
				if (result == CommandBarKeyResult::Submitted)
				{
					CommandResult cmd = _commandBarView.Submit();
					return ApplyCommandResult(cmd);
				}
				return EventResult::Consumed;
			}
			case Focus::Editor:
			{
				EditorViewKeyResult result = _editorView.HandleKey(key);
				if (result == EditorViewKeyResult::RequestCommandBar)
				{
					_commandBarView.SetText(":");
					_focus = Focus::CommandBar;
				}
				return EventResult::Consumed;
			}
			case Focus::Hierarchy:
			{
				HierarchyViewKeyResult result = _hierarchyView.HandleKey(key, world);
				if (result == HierarchyViewKeyResult::RequestCommandBar)
				{
					_commandBarView.SetText(":");
					_focus = Focus::CommandBar;
				}
				else if (result == HierarchyViewKeyResult::RequestEntityEditor)
				{
					SetSelection(_hierarchyView.GetCursorEntity(world));
					_focus = Focus::EntityEditor;
				}
				return EventResult::Consumed;
			}
			case Focus::EntityEditor:
			{
				virasa::ecs::Entity cursorEntity = _hierarchyView.GetCursorEntity(world);
				EntityEditorViewKeyResult result = _entityEditorView.HandleKey(key, const_cast<virasa::ecs::World&>(world), cursorEntity);
				if (result == EntityEditorViewKeyResult::RequestCommandBar)
				{
					_commandBarView.SetText(":");
					_focus = Focus::CommandBar;
				}
				else if (result == EntityEditorViewKeyResult::RequestHierarchy)
				{
					_focus = Focus::Hierarchy;
				}
				return EventResult::Consumed;
			}
		}
	}
	else if (event.type == virasa::EventType::TextInput)
	{
		std::string_view utf8(event.textInput.utf8, event.textInput.length);

		switch (_focus)
		{
			case Focus::CommandBar:
			{
				CommandBarKeyResult result = _commandBarView.HandleTextInput(utf8);
				if (result == CommandBarKeyResult::Submitted)
				{
					CommandResult cmd = _commandBarView.Submit();
					return ApplyCommandResult(cmd);
				}
				return EventResult::Consumed;
			}
			case Focus::Editor:
			{
				EditorViewKeyResult result = _editorView.HandleTextInput(utf8);
				if (result == EditorViewKeyResult::RequestCommandBar)
				{
					_commandBarView.SetText(":");
					_focus = Focus::CommandBar;
				}
				return EventResult::Consumed;
			}
			case Focus::Hierarchy:
			{
				HierarchyViewKeyResult result = _hierarchyView.HandleTextInput(utf8, world);
				if (result == HierarchyViewKeyResult::RequestCommandBar)
				{
					_commandBarView.SetText(":");
					_focus = Focus::CommandBar;
				}
				else if (result == HierarchyViewKeyResult::RequestEntityEditor)
				{
					SetSelection(_hierarchyView.GetCursorEntity(world));
					_focus = Focus::EntityEditor;
				}
				return EventResult::Consumed;
			}
			case Focus::EntityEditor:
			{
				virasa::ecs::Entity cursorEntity = _hierarchyView.GetCursorEntity(world);
				EntityEditorViewKeyResult result = _entityEditorView.HandleTextInput(utf8, const_cast<virasa::ecs::World&>(world), cursorEntity);
				if (result == EntityEditorViewKeyResult::RequestCommandBar)
				{
					_commandBarView.SetText(":");
					_focus = Focus::CommandBar;
				}
				else if (result == EntityEditorViewKeyResult::RequestHierarchy)
				{
					_focus = Focus::Hierarchy;
				}
				return EventResult::Consumed;
			}
		}
	}

	return EventResult::Consumed;
}

EventResult ViewManager::ApplyCommandResult(CommandResult cmd)
{
	switch (cmd)
	{
		case CommandResult::None:
			return EventResult::Consumed;

		case CommandResult::OpenEditor:
			if (_rightPanelMode == RightPanelMode::Editor)
			{
				_rightPanelMode = RightPanelMode::Closed;
				_focus = Focus::CommandBar;
			}
			else
			{
				_rightPanelMode = RightPanelMode::Editor;
				_focus = Focus::Editor;
				_editorView.GetMotionState().SetMode(virasa::editor::Mode::Normal);
			}
			return EventResult::Consumed;

		case CommandResult::OpenHierarchy:
			if (_rightPanelMode == RightPanelMode::Hierarchy)
			{
				_rightPanelMode = RightPanelMode::Closed;
				_focus = Focus::CommandBar;
			}
			else
			{
				_rightPanelMode = RightPanelMode::Hierarchy;
				_focus = Focus::Hierarchy;
			}
			return EventResult::Consumed;

		case CommandResult::Close:
			_rightPanelMode = RightPanelMode::Closed;
			_focus = Focus::CommandBar;
			return EventResult::Consumed;

		case CommandResult::Quit:
			_rightPanelMode = RightPanelMode::Closed;
			_focus = Focus::CommandBar;
			return EventResult::QuitRequested;

		case CommandResult::LoadModel:
			_pendingLoadPath = std::string(_commandBarView.GetSubmittedArgument());
			if (IsSceneLoadPath(_pendingLoadPath))
			{
				return EventResult::LoadSceneRequested;
			}
			return EventResult::LoadModelRequested;

		case CommandResult::Play:
			return EventResult::PlayRequested;

		case CommandResult::Stop:
			return EventResult::StopRequested;

		case CommandResult::SaveScene:
			_pendingSavePath = std::string(_commandBarView.GetSubmittedArgument());
			return EventResult::SaveSceneRequested;
	}

	return EventResult::Consumed;
}

void ViewManager::Render(
	virasa::ui::DrawList& out,
	const virasa::ecs::World& world,
	const virasa::ui::FontAtlas& atlas,
	uint32_t windowWidth,
	uint32_t windowHeight
)
{
	// Compute command bar height
	const float barHeightRaw =
		atlas.GetAscender() - atlas.GetDescender()
		+ 2.0f * _commandBarView.GetPanel().GetConfig().paddingY;
	const float barHeight = (barHeightRaw < static_cast<float>(windowHeight))
		? barHeightRaw
		: static_cast<float>(windowHeight);

	// Render right panel if open
	if (_rightPanelMode != RightPanelMode::Closed)
	{
		const uint32_t panelX = windowWidth / 2u;
		const uint32_t panelWidth = windowWidth - panelX;
		const float panelY = 0.0f;
		const float panelHeightRaw = static_cast<float>(windowHeight) - barHeight;
		const float panelHeight = (panelHeightRaw > 0.0f) ? panelHeightRaw : 0.0f;

		if (_rightPanelMode == RightPanelMode::Editor)
		{
			_editorView.Render(
				out,
				atlas,
				static_cast<float>(panelX),
				panelY,
				static_cast<float>(panelWidth),
				panelHeight
			);
		}
		else if (_rightPanelMode == RightPanelMode::Hierarchy)
		{
			const float hierarchyHeight = std::floor(panelHeight * 0.5f);
			const float editorY = panelY + hierarchyHeight;
			const float editorHeight = panelHeight - hierarchyHeight;

			_hierarchyView.Render(
				out,
				world,
				atlas,
				static_cast<float>(panelX),
				panelY,
				static_cast<float>(panelWidth),
				hierarchyHeight
			);

			_entityEditorView.Render(
				out,
				world,
				_hierarchyView.GetCursorEntity(world),
				atlas,
				static_cast<float>(panelX),
				editorY,
				static_cast<float>(panelWidth),
				editorHeight
			);
		}
	}

	// Always render the command bar
	_commandBarView.Render(out, atlas, windowWidth, windowHeight);
}

} // namespace virasa::editor
