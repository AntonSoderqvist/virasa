#include "virasa/ui/EntityEditorPanel.h"

namespace virasa::ui
{

void EntityEditorPanel::SetConfig(const EntityEditorPanelConfig& config) noexcept
{
	_config = config;
}

const EntityEditorPanelConfig& EntityEditorPanel::GetConfig() const noexcept
{
	return _config;
}

void EntityEditorPanel::Render(
	virasa::ui::DrawList& out,
	std::span<const EntityEditorPanelRow> rows,
	const virasa::ui::FontAtlas& atlas,
	float x,
	float y,
	float width,
	float height) const
{
	// Append the background quad.
	virasa::ui::QuadCommand bg;
	bg.x = x;
	bg.y = y;
	bg.width = width;
	bg.height = height;
	bg.color = _config.background;
	out.AddQuad(bg);

	// Compute the baseline-to-baseline advance.
	const float advance = (atlas.GetAscender() - atlas.GetDescender()) * _config.lineSpacing;

	// Emit per-row text commands.
	for (std::size_t k = 0; k < rows.size(); ++k)
	{
		const EntityEditorPanelRow& row = rows[k];
		const float rowY = y + _config.paddingY + atlas.GetAscender() + static_cast<float>(k) * advance;

		if (row.kind == EntityEditorRowKind::SectionHeader)
		{
			const float rowX = x + _config.paddingX;
			out.AddText(rowX, rowY, row.caption, _config.sectionHeaderForeground);
		}
		else // EntityEditorRowKind::Field
		{
			const float captionX = x + _config.paddingX + _config.indentX;
			const float valueX   = captionX + _config.captionColumnWidth;
			out.AddText(captionX, rowY, row.caption, _config.captionForeground);
			out.AddText(valueX,   rowY, row.value,   _config.valueForeground);
		}
	}
}

} // namespace virasa::ui
