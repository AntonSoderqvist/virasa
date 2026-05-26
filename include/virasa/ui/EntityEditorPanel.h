#ifndef VIRASA_UI_ENTITY_EDITOR_PANEL_H
#define VIRASA_UI_ENTITY_EDITOR_PANEL_H

#include <cstdint>
#include <span>
#include <string_view>

#include "virasa/ui/Types.h"
#include "virasa/ui/FontAtlas.h"

namespace virasa::ui
{

/**
 * @brief Identifies the visual treatment of a row in the EntityEditorPanel.
 *
 * SectionHeader names a row that introduces a component group (e.g. "Transform").
 * Field names a row that describes one labeled value pair (e.g. "Position" : "(0, 1, 0)").
 * Future row kinds must be appended after Field; reordering existing values is a breaking change.
 */
enum class EntityEditorRowKind : uint8_t
{
	SectionHeader,
	Field
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

	/// Color used for SectionHeader-kind row captions.
	virasa::ui::Color sectionHeaderForeground = {1.0f, 1.0f, 1.0f, 1.0f};

	/// Color used for Field-kind row captions (left column).
	virasa::ui::Color captionForeground = {0.65f, 0.68f, 0.72f, 1.0f};

	/// Color used for Field-kind row values (right column).
	virasa::ui::Color valueForeground = {0.9f, 0.9f, 0.9f, 1.0f};

	/// Horizontal inset in framebuffer pixels between the panel's left edge and a SectionHeader caption.
	float paddingX = 6.0f;

	/// Vertical inset in framebuffer pixels above the first row's ascender.
	float paddingY = 4.0f;

	/// Additional horizontal inset applied to Field-kind row captions.
	float indentX = 12.0f;

	/// Horizontal distance in framebuffer pixels from the start of a Field caption to its value column.
	float captionColumnWidth = 120.0f;

	/// Unitless multiplier on the atlas's typographic line height for baseline-to-baseline distance.
	float lineSpacing = 1.0f;
};

/**
 * @brief Holds the inputs needed to render a single visible row in the EntityEditorPanel.
 *
 * caption and value are non-owning views; they must remain valid for the duration of the
 * Render call that consumes this row.
 */
struct EntityEditorPanelRow
{
public:
	/// Non-owning view over the row's caption text (UTF-8).
	std::string_view caption = {};

	/// Non-owning view over the row's value text (UTF-8). Ignored for SectionHeader rows.
	std::string_view value = {};

	/// Visual treatment of this row.
	EntityEditorRowKind kind = EntityEditorRowKind::Field;
};

/**
 * @brief A stateless panel that renders a list of entity property rows into a DrawList.
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
	 * @brief Append DrawList commands representing the panel and its rows.
	 *
	 * Appends one background QuadCommand followed by per-row TextCommands in row order.
	 * One TextCommand is emitted for SectionHeader rows; two (caption then value) for Field rows.
	 *
	 * @param out     DrawList to append commands into.
	 * @param rows    Ordered list of rows to render.
	 * @param atlas   Font atlas used to obtain ascender and descender metrics.
	 * @param x       Left edge of the panel in framebuffer pixels.
	 * @param y       Top edge of the panel in framebuffer pixels.
	 * @param width   Width of the panel in framebuffer pixels.
	 * @param height  Height of the panel in framebuffer pixels.
	 */
	void Render(
		virasa::ui::DrawList& out,
		std::span<const EntityEditorPanelRow> rows,
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
