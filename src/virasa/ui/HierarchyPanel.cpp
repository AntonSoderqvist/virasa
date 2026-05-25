#include "virasa/ui/HierarchyPanel.h"

#include <vector>
#include <utility>

namespace virasa::ui
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

/// One entry in the visible-rows list.
struct VisibleRow
{
	virasa::ecs::Entity entity;
	int depth;
};

/// Build the visible-rows list via DFS pre-order over the expanded subtrees.
std::vector<VisibleRow> BuildVisibleRows(
	const virasa::ecs::World& world,
	const std::unordered_set<uint32_t>& expanded)
{
	std::vector<VisibleRow> rows;

	// Use an explicit stack to avoid recursion overhead.
	// Stack entries: (entity, depth)
	// We push children in reverse order so that the first child is processed first.
	struct StackEntry
	{
		virasa::ecs::Entity entity;
		int depth;
	};

	std::vector<StackEntry> stack;

	const auto& roots = world.GetRoots();
	// Push roots in reverse order so first root is processed first.
	for (auto it = roots.rbegin(); it != roots.rend(); ++it)
	{
		stack.push_back({*it, 0});
	}

	while (!stack.empty())
	{
		auto [entity, depth] = stack.back();
		stack.pop_back();

		rows.push_back({entity, depth});

		// If this entity is expanded, push its children in reverse order.
		if (expanded.count(entity.index) > 0)
		{
			const auto& children = world.GetChildren(entity);
			for (auto cit = children.rbegin(); cit != children.rend(); ++cit)
			{
				stack.push_back({*cit, depth + 1});
			}
		}
	}

	return rows;
}

/// Decode the first UTF-8 codepoint from the byte sequence.
/// Returns the codepoint and the number of bytes consumed.
/// Returns {0xFFFFFFFF, 1} on invalid sequences (consume 1 byte to avoid infinite loops).
std::pair<uint32_t, std::size_t> DecodeUtf8Codepoint(std::string_view sv)
{
	if (sv.empty())
		return {0xFFFFFFFFu, 0};

	const auto b0 = static_cast<uint8_t>(sv[0]);

	if (b0 < 0x80u)
		return {static_cast<uint32_t>(b0), 1};

	if ((b0 & 0xE0u) == 0xC0u && sv.size() >= 2)
	{
		const auto b1 = static_cast<uint8_t>(sv[1]);
		if ((b1 & 0xC0u) == 0x80u)
		{
			uint32_t cp = ((b0 & 0x1Fu) << 6u) | (b1 & 0x3Fu);
			return {cp, 2};
		}
	}

	if ((b0 & 0xF0u) == 0xE0u && sv.size() >= 3)
	{
		const auto b1 = static_cast<uint8_t>(sv[1]);
		const auto b2 = static_cast<uint8_t>(sv[2]);
		if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u)
		{
			uint32_t cp = ((b0 & 0x0Fu) << 12u) | ((b1 & 0x3Fu) << 6u) | (b2 & 0x3Fu);
			return {cp, 3};
		}
	}

	if ((b0 & 0xF8u) == 0xF0u && sv.size() >= 4)
	{
		const auto b1 = static_cast<uint8_t>(sv[1]);
		const auto b2 = static_cast<uint8_t>(sv[2]);
		const auto b3 = static_cast<uint8_t>(sv[3]);
		if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u)
		{
			uint32_t cp =
				((b0 & 0x07u) << 18u) |
				((b1 & 0x3Fu) << 12u) |
				((b2 & 0x3Fu) << 6u) |
				(b3 & 0x3Fu);
			return {cp, 4};
		}
	}

	// Invalid byte — consume one byte.
	return {0xFFFFFFFFu, 1};
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// HierarchyPanel
// ---------------------------------------------------------------------------

void HierarchyPanel::SetConfig(const HierarchyPanelConfig& config) noexcept
{
	_config = config;
}

const HierarchyPanelConfig& HierarchyPanel::GetConfig() const noexcept
{
	return _config;
}

HierarchyPanelKeyResult HierarchyPanel::HandleKey(
	virasa::KeyCode /*key*/,
	const virasa::ecs::World& /*world*/)
{
	_pendingG = false;
	return HierarchyPanelKeyResult::Consumed;
}

HierarchyPanelKeyResult HierarchyPanel::HandleTextInput(
	std::string_view utf8,
	const virasa::ecs::World& world)
{
	// Build visible rows and clamp _cursorRow before processing any codepoint.
	auto rows = BuildVisibleRows(world, _expanded);
	const std::size_t n = rows.size();

	if (n == 0)
		_cursorRow = 0;
	else if (_cursorRow >= n)
		_cursorRow = n - 1;

	// Process each codepoint in the input sequence.
	std::string_view remaining = utf8;
	while (!remaining.empty())
	{
		auto [cp, consumed] = DecodeUtf8Codepoint(remaining);
		if (consumed == 0)
			break;
		remaining = remaining.substr(consumed);

		// Handle pending 'g' state.
		if (_pendingG)
		{
			if (cp == 0x0067u) // 'g'
			{
				// gg: move to first visible row.
				_cursorRow = 0;
				_pendingG = false;
				continue;
			}
			else
			{
				// Not a second 'g': clear pending and re-process this codepoint.
				_pendingG = false;
				// Fall through to normal binding dispatch below.
			}
		}

		// Normal binding dispatch.
		switch (cp)
		{
		case 0x0068u: // 'h' — collapse or walk to parent
		{
			if (n == 0)
			{
				// no-op
				break;
			}

			virasa::ecs::Entity cursorEntity = rows[_cursorRow].entity;

			if (_expanded.count(cursorEntity.index) > 0 &&
				!world.GetChildren(cursorEntity).empty())
			{
				// Collapse the subtree.
				_expanded.erase(cursorEntity.index);
			}
			else
			{
				// Walk to parent.
				virasa::ecs::Entity parentEntity = world.GetParent(cursorEntity);
				if (!parentEntity.IsValid())
				{
					// Root — no-op.
					break;
				}

				// Rebuild rows after potential collapse (none happened here, but
				// the contract says "post-collapse visible-rows list").
				// Since we didn't collapse, the list is the same.
				// Find parentEntity in the current rows list.
				for (std::size_t i = 0; i < rows.size(); ++i)
				{
					if (rows[i].entity.index == parentEntity.index &&
						rows[i].entity.generation == parentEntity.generation)
					{
						_cursorRow = i;
						break;
					}
				}
			}
			break;
		}

		case 0x006Cu: // 'l' — expand or walk to first child
		{
			if (n == 0)
				break;

			virasa::ecs::Entity cursorEntity = rows[_cursorRow].entity;
			const auto& children = world.GetChildren(cursorEntity);

			if (children.empty())
			{
				// Leaf — no-op.
				break;
			}

			if (_expanded.count(cursorEntity.index) == 0)
			{
				// Expand the subtree.
				_expanded.insert(cursorEntity.index);
			}
			else
			{
				// Already expanded — walk to first child.
				// Rebuild rows to reflect current expansion state.
				auto newRows = BuildVisibleRows(world, _expanded);
				virasa::ecs::Entity firstChild = children[0];
				for (std::size_t i = 0; i < newRows.size(); ++i)
				{
					if (newRows[i].entity.index == firstChild.index &&
						newRows[i].entity.generation == firstChild.generation)
					{
						_cursorRow = i;
						break;
					}
				}
				// Update rows for subsequent codepoints.
				rows = std::move(newRows);
			}
			break;
		}

		case 0x006Au: // 'j' — move cursor down
		{
			if (n > 0 && _cursorRow < n - 1)
				++_cursorRow;
			break;
		}

		case 0x006Bu: // 'k' — move cursor up
		{
			if (_cursorRow > 0)
				--_cursorRow;
			break;
		}

		case 0x0067u: // 'g' — first keystroke of "gg"
		{
			_pendingG = true;
			// Do NOT clear _pendingG here; continue to next codepoint.
			continue;
		}

		case 0x0047u: // 'G' — move to last visible row
		{
			_cursorRow = (n > 0) ? (n - 1) : 0;
			break;
		}

		default:
			// Unknown codepoint — silently consumed.
			break;
		}

		// Clear _pendingG after any binding that doesn't set it.
		_pendingG = false;
	}

	// If we exit the loop with _pendingG still true (e.g., the only codepoint
	// was a single 'g'), leave _pendingG set so the next call can complete "gg".

	return HierarchyPanelKeyResult::Consumed;
}

virasa::ecs::Entity HierarchyPanel::GetCursorEntity(const virasa::ecs::World& world) const
{
	const auto rows = BuildVisibleRows(world, _expanded);
	if (rows.empty() || _cursorRow >= rows.size())
		return virasa::ecs::Entity::Invalid();
	return rows[_cursorRow].entity;
}

void HierarchyPanel::Render(
	virasa::ui::DrawList& out,
	const virasa::ecs::World& world,
	const virasa::ui::FontAtlas& atlas,
	float x,
	float y,
	float width,
	float height)
{
	// 1. Background quad.
	virasa::ui::QuadCommand bgQuad;
	bgQuad.x = x;
	bgQuad.y = y;
	bgQuad.width = width;
	bgQuad.height = height;
	bgQuad.color = _config.background;
	out.AddQuad(bgQuad);

	// 2. Compute visible rows and clamp _cursorRow (mutating).
	const auto rows = BuildVisibleRows(world, _expanded);
	const std::size_t n = rows.size();

	if (n == 0)
		_cursorRow = 0;
	else if (_cursorRow >= n)
		_cursorRow = n - 1;

	const float lineHeight = atlas.GetAscender() - atlas.GetDescender();
	const float advance = lineHeight * _config.lineSpacing;

	// 3. Cursor row highlight quad (before text).
	if (n > 0)
	{
		virasa::ui::QuadCommand cursorQuad;
		cursorQuad.x = x;
		cursorQuad.y = y + _config.paddingY + static_cast<float>(_cursorRow) * advance;
		cursorQuad.width = width;
		cursorQuad.height = lineHeight;
		cursorQuad.color = _config.cursorRowBackground;
		out.AddQuad(cursorQuad);
	}

	// 4. One TextCommand per visible row.
	for (std::size_t k = 0; k < n; ++k)
	{
		const auto& row = rows[k];
		const float rowX = x + _config.paddingX + static_cast<float>(row.depth) * _config.indentX;
		const float rowY = y + _config.paddingY + atlas.GetAscender() + static_cast<float>(k) * advance;

		const auto& nameComp = world.GetNameComponent(row.entity);
		out.AddText(rowX, rowY, nameComp.name, _config.foreground);
	}
}

} // namespace virasa::ui
