#ifndef VIRASA_UI_HIERARCHYPANEL_H
#define VIRASA_UI_HIERARCHYPANEL_H

#include "virasa/ui/Types.h"
#include "virasa/ui/FontAtlas.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace virasa::ui
{

/**
 * @brief Appearance configuration for a HierarchyPanel.
 *
 * Holds all visual parameters for one HierarchyPanel instance.
 * Copyable, movable, default-constructible, and trivially destructible.
 */
struct HierarchyPanelConfig
{
public:
	/// Panel solid backdrop color in linear-float RGBA.
	virasa::ui::Color background = {0.078f, 0.082f, 0.094f, 1.0f};

	/// Color used for every visible row's name text.
	virasa::ui::Color foreground = {0.9f, 0.9f, 0.9f, 1.0f};

	/// Highlight color drawn behind the cursor row.
	virasa::ui::Color cursorRowBackground = {0.20f, 0.30f, 0.40f, 1.0f};

	/// Horizontal inset in framebuffer pixels between the panel's left edge and depth-0 row name.
	float paddingX = 6.0f;

	/// Vertical inset in framebuffer pixels above the first row's ascender.
	float paddingY = 4.0f;

	/// Per-depth horizontal indent in framebuffer pixels.
	float indentX = 12.0f;

	/// Unitless multiplier on the atlas's typographic line height.
	float lineSpacing = 1.0f;
};

/**
 * @brief A single visible row supplied to HierarchyPanel::Render.
 *
 * Holds a non-owning name view and the row's depth in the hierarchy.
 * Copyable, movable, default-constructible, and trivially destructible.
 */
struct HierarchyPanelRow
{
public:
	/// Non-owning UTF-8 name view; must remain valid for the duration of Render.
	std::string_view name = {};

	/// Depth in the hierarchy: 0 for roots, 1 for direct children, etc.
	uint32_t depth = 0u;
};

/**
 * @brief A stateless UI panel that renders a flat list of hierarchy rows.
 *
 * HierarchyPanel holds only a HierarchyPanelConfig. The caller supplies
 * the visible rows, cursor position, atlas, and panel rectangle to Render
 * each frame.
 */
class HierarchyPanel final
{
public:
	/**
	 * @brief Replace the panel's appearance configuration.
	 * @param config The new configuration to store.
	 */
	void SetConfig(const HierarchyPanelConfig& config) noexcept;

	/**
	 * @brief Return the current appearance configuration.
	 * @return Const reference to the stored HierarchyPanelConfig.
	 */
	[[nodiscard]] const HierarchyPanelConfig& GetConfig() const noexcept;

	/**
	 * @brief Render the panel into the supplied DrawList.
	 * @param out       DrawList to append commands to.
	 * @param rows      Visible rows to render, in display order.
	 * @param cursorRow Zero-based index of the cursor row (clamped internally).
	 * @param atlas     Font atlas used for glyph metrics.
	 * @param x         Left edge of the panel in framebuffer pixels.
	 * @param y         Top edge of the panel in framebuffer pixels.
	 * @param width     Width of the panel in framebuffer pixels.
	 * @param height    Height of the panel in framebuffer pixels.
	 */
	void Render(
		virasa::ui::DrawList& out,
		std::span<const HierarchyPanelRow> rows,
		std::size_t cursorRow,
		const virasa::ui::FontAtlas& atlas,
		float x,
		float y,
		float width,
		float height) const;

private:
	HierarchyPanelConfig _config = {};
};

} // namespace virasa::ui

#endif // VIRASA_UI_HIERARCHYPANEL_H
