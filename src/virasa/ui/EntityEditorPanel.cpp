#include "virasa/ui/EntityEditorPanel.h"

#include <vector>

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
	std::size_t focusedRow,
	std::size_t focusedCell,
	const virasa::ui::FontAtlas& atlas,
	float x,
	float y,
	float width,
	float height) const
{
	virasa::ui::QuadCommand backgroundQuad;
	backgroundQuad.x = x;
	backgroundQuad.y = y;
	backgroundQuad.width = width;
	backgroundQuad.height = height;
	backgroundQuad.color = _config.background;
	out.AddQuad(backgroundQuad);

	uint32_t maxSlotIndex = 0u;
	bool hasSlot = false;

	for (const EntityEditorPanelRow& row : rows)
	{
		for (const EntityEditorPanelCell& cell : row.cells)
		{
			if (cell.kind == EntityEditorCellKind::Label || cell.kind == EntityEditorCellKind::Value)
			{
				if (!hasSlot || cell.slotIndex > maxSlotIndex)
				{
					maxSlotIndex = cell.slotIndex;
					hasSlot = true;
				}
			}
		}
	}

	std::vector<float> slotWidths;
	if (hasSlot)
	{
		slotWidths.resize(static_cast<std::size_t>(maxSlotIndex) + 1u, 0.0f);

		for (const EntityEditorPanelRow& row : rows)
		{
			for (const EntityEditorPanelCell& cell : row.cells)
			{
				if (cell.kind != EntityEditorCellKind::Label && cell.kind != EntityEditorCellKind::Value)
				{
					continue;
				}

				float measuredWidth = 0.0f;
				for (unsigned char byte : cell.text)
				{
					const uint32_t codepoint = static_cast<uint32_t>(byte);
					measuredWidth += atlas.GetGlyph(codepoint).advance;
				}

				float& slotWidth = slotWidths[static_cast<std::size_t>(cell.slotIndex)];
				if (measuredWidth > slotWidth)
				{
					slotWidth = measuredWidth;
				}
			}
		}
	}

	auto ComputeSlotX = [&](uint32_t slotIndex) noexcept -> float
	{
		float slotX = x + _config.paddingX + _config.indentX;
		const std::size_t clampedCount =
			static_cast<std::size_t>(slotIndex) < slotWidths.size() ?
			static_cast<std::size_t>(slotIndex) :
			slotWidths.size();

		for (std::size_t i = 0; i < clampedCount; ++i)
		{
			slotX += slotWidths[i] + _config.cellSpacing;
		}

		if (static_cast<std::size_t>(slotIndex) > slotWidths.size())
		{
			slotX += static_cast<float>(static_cast<std::size_t>(slotIndex) - slotWidths.size()) *
				_config.cellSpacing;
		}

		return slotX;
	};

	const float advance = (atlas.GetAscender() - atlas.GetDescender()) * _config.lineSpacing;

	if (focusedRow < rows.size())
	{
		const EntityEditorPanelRow& row = rows[focusedRow];
		if (focusedCell < row.cells.size())
		{
			const EntityEditorPanelCell& cell = row.cells[focusedCell];
			if (cell.kind == EntityEditorCellKind::Value &&
				static_cast<std::size_t>(cell.slotIndex) < slotWidths.size())
			{
				virasa::ui::QuadCommand highlightQuad;
				highlightQuad.x = ComputeSlotX(cell.slotIndex);
				highlightQuad.y = y + static_cast<float>(focusedRow) * advance;
				highlightQuad.width = slotWidths[static_cast<std::size_t>(cell.slotIndex)];
				highlightQuad.height = advance;
				highlightQuad.color = _config.focusedCellBackground;
				out.AddQuad(highlightQuad);
			}
		}
	}

	for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
	{
		const EntityEditorPanelRow& row = rows[rowIndex];
		const float rowY = y + _config.paddingY + atlas.GetAscender() +
			static_cast<float>(rowIndex) * advance;

		for (const EntityEditorPanelCell& cell : row.cells)
		{
			if (cell.kind == EntityEditorCellKind::SectionLabel)
			{
				out.AddText(x + _config.paddingX, rowY, cell.text, _config.sectionHeaderForeground);
			}
			else if (cell.kind == EntityEditorCellKind::Label)
			{
				out.AddText(ComputeSlotX(cell.slotIndex), rowY, cell.text, _config.captionForeground);
			}
			else
			{
				out.AddText(ComputeSlotX(cell.slotIndex), rowY, cell.text, _config.valueForeground);
			}
		}
	}
}

} // namespace virasa::ui
