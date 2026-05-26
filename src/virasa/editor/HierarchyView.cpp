#include "virasa/editor/HierarchyView.h"

#include "virasa/ecs/Components.h"

#include <vector>
#include <string_view>

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
	// Compute visible rows and clamp cursor before processing any codepoints.
	auto rows = ComputeVisibleRows(world, _expanded);
	const std::size_t n = rows.size();
	_cursorRow = ClampCursor(_cursorRow, n);

	bool requestCommandBar = false;

	const char* pos = utf8.data();
	const char* end = utf8.data() + utf8.size();

	while (pos < end)
	{
		uint32_t cp = NextCodepoint(pos, end);
		if (cp == 0xFFFFFFFFu)
			continue;

		// Handle pending 'g' state first.
		if (_pendingG)
		{
			if (cp == 0x0067u) // 'g'
			{
				// gg — move to first row
				_cursorRow = 0u;
				_pendingG = false;
				// Recompute rows after state change (rows unchanged here)
				continue;
			}
			else
			{
				// Clear pending and reprocess this codepoint as fresh
				_pendingG = false;
				// Fall through to normal dispatch below
			}
		}

		switch (cp)
		{
		case 0x0068u: // 'h' — collapse or walk to parent
		{
			if (n == 0u)
			{
				_pendingG = false;
				break;
			}
			virasa::ecs::Entity cursorEntity = rows[_cursorRow].entity;
			if (!world.IsValid(cursorEntity))
			{
				_pendingG = false;
				break;
			}
			const bool isExpanded = _expanded.count(cursorEntity.index) > 0u;
			const bool hasChildren = !world.GetChildren(cursorEntity).empty();
			if (isExpanded && hasChildren)
			{
				// Collapse the subtree
				_expanded.erase(cursorEntity.index);
				// _cursorRow unchanged
				// Recompute rows for subsequent codepoints
				rows = ComputeVisibleRows(world, _expanded);
			}
			else
			{
				// Walk to parent
				virasa::ecs::Entity parentEntity = world.GetParent(cursorEntity);
				if (parentEntity == virasa::ecs::Entity::Invalid())
				{
					// Already a root — no-op
					_pendingG = false;
					break;
				}
				// Find parent in the post-collapse visible-rows list
				for (std::size_t i = 0u; i < rows.size(); ++i)
				{
					if (rows[i].entity.index == parentEntity.index &&
						rows[i].entity.generation == parentEntity.generation)
					{
						_cursorRow = i;
						break;
					}
				}
			}
			_pendingG = false;
			break;
		}

		case 0x006Cu: // 'l' — expand or walk to first child
		{
			if (n == 0u)
			{
				_pendingG = false;
				break;
			}
			virasa::ecs::Entity cursorEntity = rows[_cursorRow].entity;
			if (!world.IsValid(cursorEntity))
			{
				_pendingG = false;
				break;
			}
			const auto& children = world.GetChildren(cursorEntity);
			if (children.empty())
			{
				// Leaf — no-op
				_pendingG = false;
				break;
			}
			const bool isExpanded = _expanded.count(cursorEntity.index) > 0u;
			if (!isExpanded)
			{
				// Expand the subtree
				_expanded.insert(cursorEntity.index);
				// _cursorRow unchanged
				// Recompute rows for subsequent codepoints
				rows = ComputeVisibleRows(world, _expanded);
			}
			else
			{
				// Already expanded — descend to first child
				virasa::ecs::Entity firstChild = children[0];
				// Recompute rows (already expanded, rows should be current)
				for (std::size_t i = 0u; i < rows.size(); ++i)
				{
					if (rows[i].entity.index == firstChild.index &&
						rows[i].entity.generation == firstChild.generation)
					{
						_cursorRow = i;
						break;
					}
				}
			}
			_pendingG = false;
			break;
		}

		case 0x006Au: // 'j' — move cursor down
		{
			const std::size_t currentN = rows.size();
			if (currentN > 0u && _cursorRow < currentN - 1u)
				++_cursorRow;
			_pendingG = false;
			break;
		}

		case 0x006Bu: // 'k' — move cursor up
		{
			if (_cursorRow > 0u)
				--_cursorRow;
			_pendingG = false;
			break;
		}

		case 0x0067u: // 'g' — first keystroke of "gg"
		{
			// _pendingG was false here (handled above if it was true)
			_pendingG = true;
			break;
		}

		case 0x0047u: // 'G' — move cursor to last row
		{
			const std::size_t currentN = rows.size();
			_cursorRow = (currentN > 0u) ? (currentN - 1u) : 0u;
			_pendingG = false;
			break;
		}

		case 0x003Au: // ':' — request command bar
		{
			_pendingG = false;
			requestCommandBar = true;
			// Stop processing further codepoints
			goto done;
		}

		default:
		{
			// Unmatched codepoint — silently consumed, clear pendingG
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
