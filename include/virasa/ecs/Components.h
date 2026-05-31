#ifndef VIRASA_ECS_COMPONENTS_H
#define VIRASA_ECS_COMPONENTS_H

#include <cstdint>
#include <string>

#include "virasa/math/Types.h"
#include "virasa/renderer/Types.h"

namespace virasa::ecs
{

/**
 * @brief Component that holds an owned UTF-8 string name for an entity.
 *
 * The name field stores a UTF-8 byte sequence by value. A default-constructed
 * NameComponent has an empty name. NameComponent does not enforce uniqueness;
 * that is the responsibility of the surrounding World storage.
 */
struct NameComponent
{
public:
	/// Owned UTF-8 name string.
	std::string name = {};
};

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

/**
 * @brief Component describing an infinitely distant directional light source.
 *
 * The direction field is the world-space vector along which photons travel.
 * Color and intensity are used by the upload step to build the GPU light record.
 */
struct DirectionalLightComponent
{
public:
	/// World-space direction of the light rays (expected to be normalized).
	virasa::math::Vec3 direction = virasa::math::Vec3(0.0f, 0.0f, -1.0f);
	/// Linear-space RGB color of the light.
	virasa::math::Vec3 color = virasa::math::Vec3(1.0f, 1.0f, 1.0f);
	/// Scalar intensity multiplier applied to color at upload time.
	float intensity = 1.0f;
	/// Whether this light opts into the renderer's shadow budget.
	bool castsShadows = true;
};

/**
 * @brief Component describing an omnidirectional point light source.
 *
 * Position is read from the entity's TransformComponent at upload time.
 * Color, intensity, and range describe the light's appearance and falloff.
 */
struct PointLightComponent
{
public:
	/// Linear-space RGB color of the light.
	virasa::math::Vec3 color = virasa::math::Vec3(1.0f, 1.0f, 1.0f);
	/// Scalar intensity multiplier applied to color at upload time.
	float intensity = 1.0f;
	/// World-space radius beyond which the light's contribution is zero.
	float range = 10.0f;
};

/**
 * @brief Component describing a cone-restricted spot light source.
 *
 * Position and cone axis are read from the entity's TransformComponent at upload time.
 * innerConeCos and outerConeCos are cosines of the inner and outer half-angles.
 */
struct SpotLightComponent
{
public:
	/// Linear-space RGB color of the light.
	virasa::math::Vec3 color = virasa::math::Vec3(1.0f, 1.0f, 1.0f);
	/// Scalar intensity multiplier applied to color at upload time.
	float intensity = 1.0f;
	/// World-space radius beyond which the light's contribution is zero.
	float range = 10.0f;
	/// Cosine of the inner cone half-angle (~18 degrees by default).
	float innerConeCos = 0.95f;
	/// Cosine of the outer cone half-angle (~32 degrees by default).
	float outerConeCos = 0.85f;
	/// Whether this light opts into the renderer's shadow budget.
	bool castsShadows = true;
};

/**
 * @brief Component describing a viewpoint into a scene.
 *
 * Position and orientation are read from the entity's TransformComponent at
 * render time. The aspect value of 0.0f means the renderer should derive the
 * aspect ratio from the output target.
 */
struct CameraComponent
{
public:
	/// Camera domain selecting which renderer subsystem consumes this camera.
	virasa::CameraDomain domain = virasa::CameraDomain::Main;
	/// Vertical field of view in radians.
	float fovY = 0.7853981633974483f;
	/// Width divided by height, or 0.0f to use the output target's native aspect.
	float aspect = 0.0f;
	/// Positive near clip plane distance.
	float nearPlane = 0.1f;
	/// Positive far clip plane distance.
	float farPlane = 1000.0f;
};

/**
 * @brief Component that, when present on an entity, requests a visual color highlight.
 *
 * The mere presence of a HighlightComponent on an entity is the highlight signal.
 * Color and intensity describe the highlight appearance; priority is used for
 * app-level arbitration when multiple sources compete to highlight the same entity.
 */
struct HighlightComponent
{
public:
	/// Linear-space RGB color of the highlight.
	virasa::math::Vec3 color = virasa::math::Vec3(0.1f, 0.4f, 1.0f);
	/// Scalar intensity multiplier applied to color by the consuming renderer.
	float intensity = 1.0f;
	/// Relative priority of the highlight; larger values denote higher priority.
	int32_t priority = 0;
};

} // namespace virasa::ecs

#endif // VIRASA_ECS_COMPONENTS_H
