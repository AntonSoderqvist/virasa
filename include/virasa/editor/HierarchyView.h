#ifndef VIRASA_EDITOR_HIERARCHYVIEW_H
#define VIRASA_EDITOR_HIERARCHYVIEW_H

#include "virasa/ui/HierarchyPanel.h"
#include "virasa/ui/Types.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/window/Events.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_set>

namespace virasa::editor
{

/**
 * @brief Result returned by HierarchyView input handlers.
 *
 * Consumed indicates the HierarchyView handled the event and the caller
 * should not propagate it further. RequestCommandBar indicates the event
 * was a request to transfer input focus to the command bar; the caller is
 * responsible for performing that focus transfer.
 */
enum class HierarchyViewKeyResult : uint8_t
{
	Consumed,
	RequestCommandBar
};

/**
 * @brief Vim-style interactive hierarchy view over a virasa::ecs::World.
 *
 * HierarchyView owns four pieces of state: a HierarchyPanel that performs
 * stateless rendering; a cursor row index; an expanded-entity set; and a
 * pending-'g' flag used to implement the two-keystroke "gg" binding.
 *
 * The visible rows are computed on demand as a DFS pre-order traversal of
 * the hierarchy forest rooted at world.GetRoots(), expanding only those
 * entities whose index is present in _expanded.
 */
class HierarchyView final
{
public:
	/**
	 * @brief Returns a mutable reference to the owned HierarchyPanel.
	 * @return Reference to _panel.
	 */
	[[nodiscard]] virasa::ui::HierarchyPanel& GetPanel() noexcept;

	/**
	 * @brief Returns a const reference to the owned HierarchyPanel.
	 * @return Const reference to _panel.
	 */
	[[nodiscard]] const virasa::ui::HierarchyPanel& GetPanel() const noexcept;

	/**
	 * @brief Returns the zero-based visible-row index of the focused row.
	 * @return Current value of _cursorRow.
	 */
	[[nodiscard]] std::size_t GetCursorRow() const noexcept;

	/**
	 * @brief Returns the Entity at the current cursor row.
	 *
	 * Computes the visible-rows list and returns the Entity at _cursorRow.
	 * Returns Entity::Invalid() if the list is empty or _cursorRow is out
	 * of range.
	 *
	 * @param world The ECS world to query.
	 * @return Entity at cursor, or Entity::Invalid().
	 */
	[[nodiscard]] virasa::ecs::Entity GetCursorEntity(const virasa::ecs::World& world) const;

	/**
	 * @brief Handles a non-printable key event.
	 *
	 * In this contract version every key is consumed without modifying
	 * _cursorRow or _expanded. Clears _pendingG and returns Consumed.
	 *
	 * @param key The key code of the event.
	 * @param world The ECS world (accepted for forward-compatibility).
	 * @return HierarchyViewKeyResult::Consumed.
	 */
	HierarchyViewKeyResult HandleKey(virasa::KeyCode key, const virasa::ecs::World& world);

	/**
	 * @brief Handles a text-input event carrying printable UTF-8 characters.
	 *
	 * Dispatches each codepoint to the appropriate vim-style binding:
	 * h, l, j, k, g/gg, G, and ':'.
	 *
	 * @param utf8 UTF-8 bytes of the typed characters.
	 * @param world The ECS world used for hierarchy queries.
	 * @return RequestCommandBar if ':' was encountered, otherwise Consumed.
	 */
	HierarchyViewKeyResult HandleTextInput(std::string_view utf8, const virasa::ecs::World& world);

	/**
	 * @brief Renders the hierarchy view into the supplied DrawList.
	 *
	 * Clamps _cursorRow, builds the visible-rows list, and delegates to
	 * _panel.Render. This is the only mutation Render performs (_cursorRow
	 * may be updated by the clamp).
	 *
	 * @param out DrawList to append draw commands into.
	 * @param world The ECS world to traverse.
	 * @param atlas Font atlas used for text metrics.
	 * @param x Left edge of the panel in framebuffer pixels.
	 * @param y Top edge of the panel in framebuffer pixels.
	 * @param width Width of the panel in framebuffer pixels.
	 * @param height Height of the panel in framebuffer pixels.
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
	virasa::ui::HierarchyPanel _panel = {};
	std::size_t _cursorRow = 0u;
	std::unordered_set<uint32_t> _expanded = {};
	bool _pendingG = false;
};

} // namespace virasa::editor

#endif // VIRASA_EDITOR_HIERARCHYVIEW_H
