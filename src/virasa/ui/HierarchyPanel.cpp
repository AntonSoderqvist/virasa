#include "virasa/ui/HierarchyPanel.h"

#include <cstddef>

namespace virasa::ui
{

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

void HierarchyPanel::Render(virasa::ui::DrawList& out, std::span<const HierarchyPanelRow> rows,
	std::size_t cursorRow, const virasa::ui::FontAtlas& atlas, float x, float y, float width,
	float height) const
{
	// 1. Background quad.
	virasa::ui::QuadCommand bgQuad;
	bgQuad.x = x;
	bgQuad.y = y;
	bgQuad.width = width;
	bgQuad.height = height;
	bgQuad.color = _config.background;
	out.AddQuad(bgQuad);

	const std::size_t n = rows.size();

	const float lineHeight = atlas.GetAscender() - atlas.GetDescender();
	const float advance = lineHeight * _config.lineSpacing;

	// 2. Cursor row highlight quad (before text).
	if (n > 0)
	{
		const std::size_t clampedCursorRow = (cursorRow < n) ? cursorRow : (n - 1);

		virasa::ui::QuadCommand cursorQuad;
		cursorQuad.x = x;
		cursorQuad.y = y + _config.paddingY + static_cast<float>(clampedCursorRow) * advance;
		cursorQuad.width = width;
		cursorQuad.height = lineHeight;
		cursorQuad.color = _config.cursorRowBackground;
		out.AddQuad(cursorQuad);
	}

	// 3. One TextCommand per visible row.
	for (std::size_t k = 0; k < n; ++k)
	{
		const HierarchyPanelRow& row = rows[k];
		const float rowX =
			x + _config.paddingX + static_cast<float>(row.depth) * _config.indentX;
		const float rowY =
			y + _config.paddingY + atlas.GetAscender() + static_cast<float>(k) * advance;
		out.AddText(rowX, rowY, row.name, _config.foreground);
	}
}

} // namespace virasa::ui
