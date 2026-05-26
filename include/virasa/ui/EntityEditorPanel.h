#ifndef VIRASA_UI_ENTITY_EDITOR_PANEL_H
#define VIRASA_UI_ENTITY_EDITOR_PANEL_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "virasa/ui/Types.h"
#include "virasa/ui/FontAtlas.h"

namespace virasa::ui
{

/**
 * @brief Identifies the visual treatment of a cell in the EntityEditorPanel.
 *
 * SectionLabel names a cell that introduces a component group (e.g. "Transform").
 * Label names a cell that carries non-editable text such as a field name or sub-label.
 * Value names a cell that carries conceptually editable text.
 * Future cell kinds must be appended after Value; reordering existing values is a breaking change.
 */
enum class EntityEditorCellKind : uint8_t
{
	SectionLabel,
	Label,
	Value
};

/**
 * @brief Configuration for one EntityEditorPanel's appearance.
 *
 * Holds colors, padding, indentation, and spacing values used by Render.
 * All members have sensible defaults; construct and override only what you need.
 */
struct EntityEditorPanelConfig
{
public:
	/// Panel solid backdrop color in linear-float RGBA.
	virasa::ui::Color background = {0.078f, 0.082f, 0.094f, 1.0f};

	/// Color used for SectionLabel cell text.
	virasa::ui::Color sectionHeaderForeground = {1.0f, 1.0f, 1.0f, 1.0f};

	/// Color used for Label cell text.
	virasa::ui::Color captionForeground = {0.65f, 0.68f, 0.72f, 1.0f};

	/// Color used for Value cell text.
	virasa::ui::Color valueForeground = {0.9f, 0.9f, 0.9f, 1.0f};

	/// Color used for the focused Value cell highlight quad.
	virasa::ui::Color focusedCellBackground = {0.18f, 0.22f, 0.32f, 1.0f};

	/// Horizontal inset in framebuffer pixels between the panel's left edge and the first glyph.
	float paddingX = 6.0f;

	/// Vertical inset in framebuffer pixels above the first row's ascender.
	float paddingY = 4.0f;

	/// Additional horizontal inset applied to Label and Value cells.
	float indentX = 12.0f;

	/// Horizontal gap in framebuffer pixels between adjacent slots.
	float cellSpacing = 8.0f;

	/// Unitless multiplier on the atlas's typographic line height for baseline-to-baseline distance.
	float lineSpacing = 1.0f;
};

/**
 * @brief Holds the inputs needed to render a single cell within a row.
 *
 * text is a non-owning UTF-8 view that must remain valid for the duration of the Render call.
 */
struct EntityEditorPanelCell
{
public:
	/// Visual treatment of the cell.
	EntityEditorCellKind kind = EntityEditorCellKind::Label;

	/// Non-owning view over the cell text bytes.
	std::string_view text = {};

	/// Slot index used to determine the column position for Label and Value cells.
	uint32_t slotIndex = 0u;
};

/**
 * @brief Holds the inputs needed to render a single visible row in the EntityEditorPanel.
 *
 * cells is a non-owning span that must remain valid for the duration of the Render call.
 */
struct EntityEditorPanelRow
{
public:
	/// Non-owning view over the row's cells.
	std::span<const EntityEditorPanelCell> cells = {};
};

/**
 * @brief A stateless panel that renders entity-editor rows into a DrawList.
 *
 * EntityEditorPanel holds only its configuration. The caller supplies the row data
 * to Render every frame; the panel retains no per-frame state.
 */
class EntityEditorPanel final
{
public:
	/**
	 * @brief Replace the panel's configuration.
	 * @param config The new configuration to store.
	 */
	void SetConfig(const EntityEditorPanelConfig& config) noexcept;

	/**
	 * @brief Return the panel's current configuration.
	 * @return A const reference to the stored EntityEditorPanelConfig.
	 */
	[[nodiscard]] const EntityEditorPanelConfig& GetConfig() const noexcept;

	/**
	 * @brief Append DrawList commands representing the panel and its cells.
	 * @param out DrawList to append commands into.
	 * @param rows Ordered list of rows to render.
	 * @param focusedRow Zero-based focused row index, or max size_t for no focus.
	 * @param focusedCell Zero-based focused cell index within the focused row, or max size_t for no focus.
	 * @param atlas Font atlas used to measure text and obtain face metrics.
	 * @param x Left edge of the panel in framebuffer pixels.
	 * @param y Top edge of the panel in framebuffer pixels.
	 * @param width Width of the panel in framebuffer pixels.
	 * @param height Height of the panel in framebuffer pixels.
	 */
	void Render(
		virasa::ui::DrawList& out,
		std::span<const EntityEditorPanelRow> rows,
		std::size_t focusedRow,
		std::size_t focusedCell,
		const virasa::ui::FontAtlas& atlas,
		float x,
		float y,
		float width,
		float height) const;

private:
	EntityEditorPanelConfig _config = {};
};

} // namespace virasa::ui

#endif // VIRASA_UI_ENTITY_EDITOR_PANEL_H
