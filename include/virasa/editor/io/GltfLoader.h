#ifndef VIRASA_EDITOR_IO_GLTFLOADER_H
#define VIRASA_EDITOR_IO_GLTFLOADER_H

#include <cstdint>
#include <string>

#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/renderer/scene/SceneRenderer.h"

namespace virasa
{
namespace editor::io
{
	/**
	 * @brief Error categories returned by GLB loading.
	 */
	enum class GltfLoadError : uint8_t
	{
		None = 0,
		FileNotFound,
		ParseFailed,
		UnsupportedFeature,
		TextureDecodeFailed,
		TextureRegistrationFailed,
		MaterialRegistrationFailed,
		MeshRegistrationFailed
	};

	/**
	 * @brief Result returned by LoadGlb.
	 */
	struct GltfLoadResult
	{
	public:
		virasa::ecs::Entity root = virasa::ecs::Entity::Invalid();
		virasa::editor::io::GltfLoadError error = virasa::editor::io::GltfLoadError::None;
	};

	/**
	 * @brief Load a GLB asset into the supplied world and scene renderer.
	 * @param path Path to the .glb file.
	 * @param world Destination ECS world.
	 * @param scene_renderer Renderer resource-registration facade.
	 * @return The created root entity on success, or an error category on failure.
	 */
	[[nodiscard]] virasa::editor::io::GltfLoadResult LoadGlb(
		const std::string& path,
		virasa::ecs::World& world,
		virasa::renderer::scene::SceneRenderer& scene_renderer
	);
}
}

#endif
