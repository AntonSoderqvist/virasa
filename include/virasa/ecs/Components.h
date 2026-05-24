#ifndef VIRASA_ECS_COMPONENTS_H
#define VIRASA_ECS_COMPONENTS_H

#include <cstdint>

namespace virasa::ecs
{

/**
 * @brief Component that holds a non-owning reference to a Mesh in a MeshRegistry.
 *
 * The meshId field is a uint32_t that refers to a mesh slot in an external
 * MeshRegistry. A value of 0xFFFFFFFFu indicates no mesh.
 */
struct MeshComponent
{
	public:
	uint32_t meshId = 0xFFFFFFFFu;
};

/**
 * @brief Component that holds a non-owning reference to a VisualMaterial slot.
 *
 * The materialId field is a uint32_t that refers to a material in a
 * VisualMaterialTable. A value of 0xFFFFFFFFu indicates no material.
 */
struct VisualComponent
{
	public:
	uint32_t materialId = 0xFFFFFFFFu;
};

} // namespace virasa::ecs

#endif // VIRASA_ECS_COMPONENTS_H
