#ifndef VIRASA_UI_HIERARCHYPANEL_H
#define VIRASA_UI_HIERARCHYPANEL_H

#include "virasa/ui/Types.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/window/Events.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_set>

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
 * @brief Result returned by HierarchyPanel key and text-input handlers.
 *
 * Consumed indicates that the HierarchyPanel handled the input and the
 * caller should not propagate it further.
 */
enum class HierarchyPanelKeyResult : uint8_t
{
	Consumed
};

/**
 * @brief A UI panel that displays the ECS entity hierarchy and supports
 *        vim-style keyboard navigation.
 *
 * HierarchyPanel renders a depth-first pre-order view of the World's
 * entity hierarchy. It supports 'h', 'l', 'j', 'k', 'gg', and 'G'
 * bindings for navigation and collapse/expand.
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
	 * @brief Handle a non-printable key-down event.
	 * @param key    The physical key code.
	 * @param world  The ECS world (accepted for forward-compatibility).
	 * @return HierarchyPanelKeyResult::Consumed always.
	 */
	HierarchyPanelKeyResult HandleKey(virasa::KeyCode key, const virasa::ecs::World& world);

	/**
	 * @brief Handle a text-input event carrying one or more UTF-8 codepoints.
	 * @param utf8   The UTF-8 byte sequence produced by the platform.
	 * @param world  The ECS world used to query the hierarchy.
	 * @return HierarchyPanelKeyResult::Consumed always.
	 */
	HierarchyPanelKeyResult HandleTextInput(std::string_view utf8, const virasa::ecs::World& world);

	/**
	 * @brief Return the entity at the current cursor row.
	 * @param world  The ECS world used to compute the visible-rows list.
	 * @return The Entity at _cursorRow, or Entity::Invalid() if the list is
	 *         empty or _cursorRow is out of range.
	 */
	[[nodiscard]] virasa::ecs::Entity GetCursorEntity(const virasa::ecs::World& world) const;

	/**
	 * @brief Render the panel into the supplied DrawList.
	 * @param out     DrawList to append commands to.
	 * @param world   The ECS world used to compute the visible-rows list.
	 * @param atlas   Font atlas used for glyph metrics.
	 * @param x       Left edge of the panel in framebuffer pixels.
	 * @param y       Top edge of the panel in framebuffer pixels.
	 * @param width   Width of the panel in framebuffer pixels.
	 * @param height  Height of the panel in framebuffer pixels.
	 */
	void Render(
		virasa::ui::DrawList& out,
		const virasa::ecs::World& world,
		const virasa::ui::FontAtlas& atlas,
		float x,
		float y,
		float width,
		float height);

private:
	HierarchyPanelConfig _config = {};
	std::size_t _cursorRow = 0u;
	std::unordered_set<uint32_t> _expanded = {};
	bool _pendingG = false;
};

} // namespace virasa::ui

#endif // VIRASA_UI_HIERARCHYPANEL_H
