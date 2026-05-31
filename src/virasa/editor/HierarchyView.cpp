#include "virasa/editor/HierarchyView.h"

#include "virasa/ecs/Components.h"

#include <vector>

namespace virasa::editor
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

struct VisibleRow
{
	virasa::ecs::Entity entity;
	uint32_t depth;
};

/**
 * Recursively appends rows in DFS pre-order.
 */
void BuildVisibleRows(
	const virasa::ecs::World& world,
	virasa::ecs::Entity entity,
	uint32_t depth,
	const std::unordered_set<uint32_t>& expanded,
	std::vector<VisibleRow>& rows)
{
	rows.push_back({entity, depth});

	if (expanded.count(entity.index) > 0)
	{
		const auto& children = world.GetChildren(entity);
		for (const auto& child : children)
		{
			BuildVisibleRows(world, child, depth + 1u, expanded, rows);
		}
	}
}

std::vector<VisibleRow> ComputeVisibleRows(
	const virasa::ecs::World& world,
	const std::unordered_set<uint32_t>& expanded)
{
	std::vector<VisibleRow> rows;
	const auto& roots = world.GetRoots();
	for (const auto& root : roots)
	{
		BuildVisibleRows(world, root, 0u, expanded, rows);
	}
	return rows;
}

std::size_t FindVisibleRowIndex(
	const std::vector<VisibleRow>& rows,
	virasa::ecs::Entity entity)
{
	for (std::size_t i = 0u; i < rows.size(); ++i)
	{
		if (rows[i].entity == entity)
		{
			return i;
		}
	}

	return rows.size();
}

/**
 * Clamps _cursorRow to [0, count - 1], or 0 if count == 0.
 */
std::size_t ClampCursor(std::size_t cursor, std::size_t count)
{
	if (count == 0u)
		return 0u;
	if (cursor >= count)
		return count - 1u;
	return cursor;
}

/**
 * Decode the next UTF-8 codepoint from [pos, end).
 * Returns the codepoint and advances pos past the bytes consumed.
 * Returns 0xFFFFFFFF on invalid/empty input.
 */
uint32_t NextCodepoint(const char*& pos, const char* end)
{
	if (pos >= end)
		return 0xFFFFFFFFu;

	unsigned char c = static_cast<unsigned char>(*pos);
	uint32_t cp = 0u;
	int extra = 0;

	if (c < 0x80u)
	{
		cp = c;
		extra = 0;
	}
	else if ((c & 0xE0u) == 0xC0u)
	{
		cp = c & 0x1Fu;
		extra = 1;
	}
	else if ((c & 0xF0u) == 0xE0u)
	{
		cp = c & 0x0Fu;
		extra = 2;
	}
	else if ((c & 0xF8u) == 0xF0u)
	{
		cp = c & 0x07u;
		extra = 3;
	}
	else
	{
		// Invalid leading byte — skip one byte
		++pos;
		return 0xFFFFFFFFu;
	}

	++pos;
	for (int i = 0; i < extra; ++i)
	{
		if (pos >= end)
			return 0xFFFFFFFFu;
		unsigned char cont = static_cast<unsigned char>(*pos);
		if ((cont & 0xC0u) != 0x80u)
			return 0xFFFFFFFFu;
		cp = (cp << 6u) | (cont & 0x3Fu);
		++pos;
	}

	return cp;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// HierarchyView — public interface
// ---------------------------------------------------------------------------

virasa::ui::HierarchyPanel& HierarchyView::GetPanel() noexcept
{
	return _panel;
}

const virasa::ui::HierarchyPanel& HierarchyView::GetPanel() const noexcept
{
	return _panel;
}

std::size_t HierarchyView::GetCursorRow() const noexcept
{
	return _cursorRow;
}

virasa::ecs::Entity HierarchyView::GetCursorEntity(const virasa::ecs::World& world) const
{
	auto rows = ComputeVisibleRows(world, _expanded);
	if (rows.empty() || _cursorRow >= rows.size())
		return virasa::ecs::Entity::Invalid();
	return rows[_cursorRow].entity;
}

void HierarchyView::SetCursorToEntity(
	const virasa::ecs::World& world,
	virasa::ecs::Entity entity)
{
	if (entity == virasa::ecs::Entity::Invalid())
	{
		return;
	}

	virasa::ecs::Entity parent = world.GetParent(entity);
	while (parent != virasa::ecs::Entity::Invalid())
	{
		_expanded.insert(parent.index);
		parent = world.GetParent(parent);
	}

	auto rows = ComputeVisibleRows(world, _expanded);
	const std::size_t rowIndex = FindVisibleRowIndex(rows, entity);
	if (rowIndex != rows.size())
	{
		_cursorRow = rowIndex;
	}

	_pendingG = false;
}

HierarchyViewKeyResult HierarchyView::HandleKey(
	virasa::KeyCode key,
	const virasa::ecs::World& world)
{
	if (key == virasa::KeyCode::Enter)
	{
		// Look up the cursor entity.
		auto rows = ComputeVisibleRows(world, _expanded);
		if (rows.empty() || _cursorRow >= rows.size())
		{
			_pendingG = false;
			return HierarchyViewKeyResult::Consumed;
		}
		_pendingG = false;
		return HierarchyViewKeyResult::RequestEntityEditor;
	}

	_pendingG = false;
	return HierarchyViewKeyResult::Consumed;
}

HierarchyViewKeyResult HierarchyView::HandleTextInput(
	std::string_view utf8,
	const virasa::ecs::World& world)
{
	auto rows = ComputeVisibleRows(world, _expanded);
	_cursorRow = ClampCursor(_cursorRow, rows.size());

	bool requestCommandBar = false;

	const char* pos = utf8.data();
	const char* end = utf8.data() + utf8.size();

	while (pos < end)
	{
		uint32_t cp = NextCodepoint(pos, end);
		if (cp == 0xFFFFFFFFu)
		{
			continue;
		}

		if (_pendingG && cp != 0x0067u)
		{
			_pendingG = false;
		}
		else if (_pendingG && cp == 0x0067u)
		{
			_cursorRow = 0u;
			_pendingG = false;
			continue;
		}

		switch (cp)
		{
		case 0x0068u:
		{
			if (!rows.empty())
			{
				const virasa::ecs::Entity cursorEntity = rows[_cursorRow].entity;
				const bool isExpanded = _expanded.count(cursorEntity.index) > 0u;
				const bool hasChildren = !world.GetChildren(cursorEntity).empty();
				if (isExpanded && hasChildren)
				{
					_expanded.erase(cursorEntity.index);
					rows = ComputeVisibleRows(world, _expanded);
				}
				else
				{
					const virasa::ecs::Entity parentEntity = world.GetParent(cursorEntity);
					if (parentEntity != virasa::ecs::Entity::Invalid())
					{
						const std::size_t parentRow = FindVisibleRowIndex(rows, parentEntity);
						if (parentRow != rows.size())
						{
							_cursorRow = parentRow;
						}
					}
				}
			}
			_pendingG = false;
			break;
		}

		case 0x006Cu:
		{
			if (!rows.empty())
			{
				const virasa::ecs::Entity cursorEntity = rows[_cursorRow].entity;
				const auto& children = world.GetChildren(cursorEntity);
				if (!children.empty())
				{
					const bool isExpanded = _expanded.count(cursorEntity.index) > 0u;
					if (!isExpanded)
					{
						_expanded.insert(cursorEntity.index);
						rows = ComputeVisibleRows(world, _expanded);
					}
					else
					{
						const std::size_t childRow = FindVisibleRowIndex(rows, children.front());
						if (childRow != rows.size())
						{
							_cursorRow = childRow;
						}
					}
				}
			}
			_pendingG = false;
			break;
		}

		case 0x006Au:
		{
			if (!rows.empty() && _cursorRow < rows.size() - 1u)
			{
				++_cursorRow;
			}
			_pendingG = false;
			break;
		}

		case 0x006Bu:
		{
			if (_cursorRow > 0u)
			{
				--_cursorRow;
			}
			_pendingG = false;
			break;
		}

		case 0x0067u:
		{
			_pendingG = true;
			break;
		}

		case 0x0047u:
		{
			_cursorRow = rows.empty() ? 0u : (rows.size() - 1u);
			_pendingG = false;
			break;
		}

		case 0x003Au:
		{
			_pendingG = false;
			requestCommandBar = true;
			goto done;
		}

		default:
		{
			_pendingG = false;
			break;
		}
		}
	}

done:
	return requestCommandBar
		? HierarchyViewKeyResult::RequestCommandBar
		: HierarchyViewKeyResult::Consumed;
}

void HierarchyView::Render(
	virasa::ui::DrawList& out,
	const virasa::ecs::World& world,
	const virasa::ui::FontAtlas& atlas,
	float x,
	float y,
	float width,
	float height)
{
	// Compute visible rows and clamp cursor.
	auto rows = ComputeVisibleRows(world, _expanded);
	const std::size_t n = rows.size();
	_cursorRow = ClampCursor(_cursorRow, n);

	// Build the HierarchyPanelRow sequence.
	std::vector<virasa::ui::HierarchyPanelRow> panelRows;
	panelRows.reserve(n);
	for (const auto& row : rows)
	{
		virasa::ui::HierarchyPanelRow panelRow;
		if (world.HasNameComponent(row.entity))
			panelRow.name = world.GetNameComponent(row.entity).name;
		else
			panelRow.name = {};
		panelRow.depth = row.depth;
		panelRows.push_back(panelRow);
	}

	_panel.Render(out, panelRows, _cursorRow, atlas, x, y, width, height);
}

} // namespace virasa::editor
