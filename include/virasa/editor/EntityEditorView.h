#ifndef VIRASA_EDITOR_ENTITY_EDITOR_VIEW_H
#define VIRASA_EDITOR_ENTITY_EDITOR_VIEW_H

#include "virasa/ui/EntityEditorPanel.h"
#include "virasa/ui/Types.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"

namespace virasa::editor
{

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
	 * @return Mutable reference to _panel.
	 */
	[[nodiscard]] virasa::ui::EntityEditorPanel& GetPanel() noexcept;

	/**
	 * @brief Returns a const reference to the owned EntityEditorPanel.
	 * @return Const reference to _panel.
	 */
	[[nodiscard]] const virasa::ui::EntityEditorPanel& GetPanel() const noexcept;

	/**
	 * @brief Builds the row buffer for entity and delegates rendering to the owned panel.
	 *
	 * If entity is invalid or stale (destroyed/recycled), only the background quad is emitted.
	 * Otherwise, a Name row is emitted first, followed by one section per present component
	 * in the fixed order: Transform, Mesh, Visual, DirectionalLight, PointLight, SpotLight, Camera.
	 *
	 * @param out     DrawList to append commands into (not cleared by this call).
	 * @param world   The ECS world owning the entity's components.
	 * @param entity  The entity to inspect.
	 * @param atlas   Font atlas used for text layout.
	 * @param x       Left edge of the panel in framebuffer pixels.
	 * @param y       Top edge of the panel in framebuffer pixels.
	 * @param width   Width of the panel in framebuffer pixels.
	 * @param height  Height of the panel in framebuffer pixels.
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
	virasa::ui::EntityEditorPanel _panel = {};
};

} // namespace virasa::editor

#endif // VIRASA_EDITOR_ENTITY_EDITOR_VIEW_H
