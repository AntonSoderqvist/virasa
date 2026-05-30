#ifndef VIRASA_EDITOR_IO_GLTFLOADER_H
#define VIRASA_EDITOR_IO_GLTFLOADER_H

#include <cstdint>
#include <string>
#include <vector>

#include "vulkan/vulkan.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/editor/io/ImageLoader.h"
#include "virasa/math/Transform.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/material/Visual.h"
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
	 * @brief CPU-resident decoded texture ready for GPU upload.
	 */
	struct GltfCpuTexture
	{
	public:
		GltfCpuTexture() = default;
		GltfCpuTexture(const GltfCpuTexture&) = delete;
		GltfCpuTexture& operator=(const GltfCpuTexture&) = delete;
		GltfCpuTexture(GltfCpuTexture&&) = default;
		GltfCpuTexture& operator=(GltfCpuTexture&&) = default;

		virasa::editor::io::DecodedImage image;
		virasa::SamplerConfig sampler;
		VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	};

	/**
	 * @brief CPU-resident material with glTF-local texture indices.
	 */
	struct GltfCpuMaterial
	{
	public:
		virasa::VisualMaterial material;
		int32_t baseColorTexture = -1;
		int32_t metallicRoughnessTexture = -1;
		int32_t normalTexture = -1;
		int32_t occlusionTexture = -1;
		int32_t emissiveTexture = -1;
	};

	/**
	 * @brief CPU-resident decoded mesh primitive ready for GPU upload.
	 */
	struct GltfCpuMesh
	{
	public:
		GltfCpuMesh() = default;
		GltfCpuMesh(const GltfCpuMesh&) = delete;
		GltfCpuMesh& operator=(const GltfCpuMesh&) = delete;
		GltfCpuMesh(GltfCpuMesh&&) = default;
		GltfCpuMesh& operator=(GltfCpuMesh&&) = default;

		virasa::MeshData data;
		uint32_t meshIndex = 0u;
		uint32_t primitiveIndex = 0u;
		int32_t material = -1;
	};

	/**
	 * @brief CPU-resident glTF scene node descriptor.
	 */
	struct GltfCpuNode
	{
	public:
		std::string name;
		virasa::math::Transform transform;
		int32_t parent = -1;
		int32_t meshIndex = -1;
	};

	/**
	 * @brief Top-level CPU-resident intermediate representation of one GLB file.
	 */
	struct GltfCpuAsset
	{
	public:
		GltfCpuAsset() = default;
		GltfCpuAsset(const GltfCpuAsset&) = delete;
		GltfCpuAsset& operator=(const GltfCpuAsset&) = delete;
		GltfCpuAsset(GltfCpuAsset&&) = default;
		GltfCpuAsset& operator=(GltfCpuAsset&&) = default;

		std::vector<virasa::editor::io::GltfCpuTexture> textures;
		std::vector<virasa::editor::io::GltfCpuMaterial> materials;
		std::vector<virasa::editor::io::GltfCpuMesh> meshes;
		std::vector<virasa::editor::io::GltfCpuNode> nodes;
		virasa::editor::io::GltfLoadError error = virasa::editor::io::GltfLoadError::None;
	};

	/**
	 * @brief Result returned by CommitGlb and LoadGlb.
	 */
	struct GltfLoadResult
	{
	public:
		virasa::ecs::Entity root = virasa::ecs::Entity::Invalid();
		virasa::editor::io::GltfLoadError error = virasa::editor::io::GltfLoadError::None;
	};

	/**
	 * @brief Parse a GLB file into a CPU-resident intermediate asset.
	 * @param path Path to the .glb file.
	 * @return A GltfCpuAsset; check error == None before passing to CommitGlb.
	 */
	[[nodiscard]] virasa::editor::io::GltfCpuAsset ParseGlb(const std::string& path);

	/**
	 * @brief Register a parsed CPU asset with the SceneRenderer and build ECS entities.
	 * @param asset A successfully-parsed GltfCpuAsset (asset.error must be None).
	 * @param world Destination ECS world.
	 * @param scene_renderer Renderer resource-registration facade.
	 * @return GltfLoadResult with the axis-bake root entity on success.
	 */
	[[nodiscard]] virasa::editor::io::GltfLoadResult CommitGlb(
		const virasa::editor::io::GltfCpuAsset& asset,
		virasa::ecs::World& world,
		virasa::renderer::scene::SceneRenderer& scene_renderer
	);

	/**
	 * @brief Synchronous convenience entry point: ParseGlb + CommitGlb.
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
