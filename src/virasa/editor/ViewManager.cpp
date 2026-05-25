#include "virasa/editor/ViewManager.h"

#include "virasa/editor/Motions.h"

namespace virasa::editor
{

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
				return EventResult::Consumed;
			}
		}
	}

	return EventResult::Consumed;
}

EventResult ViewManager::ApplyCommandResult(CommandResult cmd) noexcept
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
			_hierarchyView.Render(
				out,
				world,
				atlas,
				static_cast<float>(panelX),
				panelY,
				static_cast<float>(panelWidth),
				panelHeight
			);
		}
	}

	// Always render the command bar
	_commandBarView.Render(out, atlas, windowWidth, windowHeight);
}

} // namespace virasa::editor
