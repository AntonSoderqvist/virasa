#ifndef VIRASA_EDITOR_ENTITY_EDITOR_VIEW_H
#define VIRASA_EDITOR_ENTITY_EDITOR_VIEW_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>

#include "virasa/ui/EntityEditorPanel.h"
#include "virasa/ui/Types.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/window/Events.h"

namespace virasa::editor
{

/**
 * @brief Result of handling an input event in EntityEditorView.
 */
enum class EntityEditorViewKeyResult : uint8_t
{
	Consumed,
	RequestHierarchy,
	RequestCommandBar,
};

struct BuiltRows;

class EntityEditorView;

void ClampCursorToBuiltRows(EntityEditorView& view, const BuiltRows& built);
BuiltRows BuildRows(const virasa::ecs::World& world, virasa::ecs::Entity entity, const EntityEditorView& view);
void EnterEditMode(EntityEditorView& view, const BuiltRows& built, const virasa::ecs::World& world, virasa::ecs::Entity entity);
void CommitEdit(EntityEditorView& view, virasa::ecs::World& world, virasa::ecs::Entity entity);
void OverlayEditBuffer(EntityEditorView& view, BuiltRows& built);

/**
 * @brief Inspects a single ECS entity and renders its components as a panel of labeled rows.
 *
 * EntityEditorView owns a virasa::ui::EntityEditorPanel and builds the per-frame row buffer
 * from the entity supplied to Render each call. The bound entity is not stored; the caller
 * passes it in every frame, allowing the caller (e.g. ViewManager) to wire it to whichever
 * source it chooses without coupling the views directly.
 *
 * EntityEditorView owns no Vulkan resources and performs no Vulkan API calls.
 */
class EntityEditorView final
{
public:
	/**
	 * @brief Returns a mutable reference to the owned EntityEditorPanel.
	 * @return Mutable reference to the owned panel.
	 */
	[[nodiscard]] virasa::ui::EntityEditorPanel& GetPanel() noexcept;

	/**
	 * @brief Returns a const reference to the owned EntityEditorPanel.
	 * @return Const reference to the owned panel.
	 */
	[[nodiscard]] const virasa::ui::EntityEditorPanel& GetPanel() const noexcept;

	/**
	 * @brief Returns the current focused visible-row index.
	 * @return Current cursor row.
	 */
	[[nodiscard]] std::size_t GetCursorRow() const noexcept;

	/**
	 * @brief Returns the current focused cell index within the focused row.
	 * @return Current cursor cell.
	 */
	[[nodiscard]] std::size_t GetCursorCell() const noexcept;

	/**
	 * @brief Returns whether the view is currently editing a single value cell.
	 * @return True when edit mode is active.
	 */
	[[nodiscard]] bool IsEditing() const noexcept;

	/**
	 * @brief Handles a non-text key press.
	 * @param key Physical key code.
	 * @param world ECS world containing the entity and components.
	 * @param entity Entity being inspected.
	 * @return Result describing whether focus transfer was requested.
	 */
	[[nodiscard]] EntityEditorViewKeyResult HandleKey(
		virasa::KeyCode key,
		virasa::ecs::World& world,
		virasa::ecs::Entity entity);

	/**
	 * @brief Handles UTF-8 text input.
	 * @param utf8 UTF-8 bytes supplied by the input system.
	 * @param world ECS world containing the entity and components.
	 * @param entity Entity being inspected.
	 * @return Result describing whether focus transfer was requested.
	 */
	[[nodiscard]] EntityEditorViewKeyResult HandleTextInput(
		std::string_view utf8,
		virasa::ecs::World& world,
		virasa::ecs::Entity entity);

	/**
	 * @brief Builds the current visible rows and delegates rendering to the owned panel.
	 * @param out DrawList to append commands into.
	 * @param world ECS world containing the entity and components.
	 * @param entity Entity being inspected.
	 * @param atlas Font atlas used for text layout.
	 * @param x Left edge of the panel in framebuffer pixels.
	 * @param y Top edge of the panel in framebuffer pixels.
	 * @param width Width of the panel in framebuffer pixels.
	 * @param height Height of the panel in framebuffer pixels.
	 */
	void Render(
		virasa::ui::DrawList& out,
		const virasa::ecs::World& world,
		virasa::ecs::Entity entity,
		const virasa::ui::FontAtlas& atlas,
		float x,
		float y,
		float width,
		float height);

private:
	friend void ClampCursorToBuiltRows(EntityEditorView& view, const BuiltRows& built);
	friend BuiltRows BuildRows(const virasa::ecs::World& world, virasa::ecs::Entity entity, const EntityEditorView& view);
	friend void EnterEditMode(EntityEditorView& view, const BuiltRows& built, const virasa::ecs::World& world, virasa::ecs::Entity entity);
	friend void CommitEdit(EntityEditorView& view, virasa::ecs::World& world, virasa::ecs::Entity entity);
	friend void OverlayEditBuffer(EntityEditorView& view, BuiltRows& built);

	virasa::ui::EntityEditorPanel _panel = {};
	std::size_t _cursorRow = 0u;
	std::size_t _cursorCell = 0u;
	std::unordered_set<uint8_t> _collapsed = {};
	bool _editing = false;
	std::string _editBuffer = {};
	bool _pendingG = false;
};

} // namespace virasa::editor

#endif // VIRASA_EDITOR_ENTITY_EDITOR_VIEW_H
