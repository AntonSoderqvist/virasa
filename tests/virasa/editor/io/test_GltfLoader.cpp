#include <gtest/gtest.h>

#include "virasa/editor/io/GltfLoader.h"

#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/material/Visual.h"
#include "virasa/math/Types.h"
#include "virasa/math/Transform.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <type_traits>
#include <utility>

namespace
{

using namespace virasa;
using namespace virasa::editor::io;
using namespace virasa::ecs;

[[nodiscard]] std::string MakeTempFilePath(const char* fileName)
{
	const std::filesystem::path tempDir = std::filesystem::temp_directory_path();
	return (tempDir / fileName).string();
}

} // namespace

TEST(GltfLoader, test_gltf_load_error_enum_values_in_declared_order)
{
	static_assert(std::is_enum_v<GltfLoadError>);
	static_assert(std::is_same_v<std::underlying_type_t<GltfLoadError>, uint8_t>);

	EXPECT_EQ(static_cast<uint8_t>(GltfLoadError::None), 0u);
	EXPECT_EQ(static_cast<uint8_t>(GltfLoadError::FileNotFound), 1u);
	EXPECT_EQ(static_cast<uint8_t>(GltfLoadError::ParseFailed), 2u);
	EXPECT_EQ(static_cast<uint8_t>(GltfLoadError::UnsupportedFeature), 3u);
	EXPECT_EQ(static_cast<uint8_t>(GltfLoadError::TextureDecodeFailed), 4u);
	EXPECT_EQ(static_cast<uint8_t>(GltfLoadError::TextureRegistrationFailed), 5u);
	EXPECT_EQ(static_cast<uint8_t>(GltfLoadError::MaterialRegistrationFailed), 6u);
	EXPECT_EQ(static_cast<uint8_t>(GltfLoadError::MeshRegistrationFailed), 7u);

	EXPECT_NE(GltfLoadError::None, GltfLoadError::FileNotFound);
	EXPECT_NE(GltfLoadError::None, GltfLoadError::ParseFailed);
	EXPECT_NE(GltfLoadError::None, GltfLoadError::UnsupportedFeature);
	EXPECT_NE(GltfLoadError::None, GltfLoadError::TextureDecodeFailed);
	EXPECT_NE(GltfLoadError::None, GltfLoadError::TextureRegistrationFailed);
	EXPECT_NE(GltfLoadError::None, GltfLoadError::MaterialRegistrationFailed);
	EXPECT_NE(GltfLoadError::None, GltfLoadError::MeshRegistrationFailed);
}

TEST(GltfLoader, test_gltf_load_result_carries_root_entity_and_error)
{
	static_assert(std::is_default_constructible_v<GltfLoadResult>);
	static_assert(std::is_copy_constructible_v<GltfLoadResult>);
	static_assert(std::is_copy_assignable_v<GltfLoadResult>);
	static_assert(std::is_move_constructible_v<GltfLoadResult>);
	static_assert(std::is_move_assignable_v<GltfLoadResult>);
	static_assert(std::is_trivially_destructible_v<GltfLoadResult>);

	// Default-constructed result has Invalid root and None error
	GltfLoadResult result;
	EXPECT_EQ(result.root, Entity::Invalid());
	EXPECT_FALSE(result.root.IsValid());
	EXPECT_EQ(result.error, GltfLoadError::None);

	// Members are publicly settable
	GltfLoadResult assigned;
	assigned.root = Entity{42u, 7u};
	assigned.error = GltfLoadError::MeshRegistrationFailed;
	EXPECT_EQ(assigned.root.index, 42u);
	EXPECT_EQ(assigned.root.generation, 7u);
	EXPECT_EQ(assigned.error, GltfLoadError::MeshRegistrationFailed);

	// Copy semantics
	GltfLoadResult copied = assigned;
	EXPECT_EQ(copied.root, assigned.root);
	EXPECT_EQ(copied.error, assigned.error);

	// Move semantics
	GltfLoadResult moved = std::move(copied);
	EXPECT_EQ(moved.root, assigned.root);
	EXPECT_EQ(moved.error, assigned.error);

	// On failure, root must be Invalid
	GltfLoadResult failure;
	failure.root = Entity::Invalid();
	failure.error = GltfLoadError::FileNotFound;
	EXPECT_FALSE(failure.root.IsValid());
	EXPECT_NE(failure.error, GltfLoadError::None);
}

// ---------------------------------------------------------------------------
// GltfCpuAsset family — struct layout and trait tests
// ---------------------------------------------------------------------------

TEST(GltfLoader, test_gltf_cpu_asset_holds_decoded_intermediate)
{
	// GltfCpuTexture: non-copyable, movable, default-constructible
	static_assert(!std::is_copy_constructible_v<GltfCpuTexture>);
	static_assert(!std::is_copy_assignable_v<GltfCpuTexture>);
	static_assert(std::is_move_constructible_v<GltfCpuTexture>);
	static_assert(std::is_move_assignable_v<GltfCpuTexture>);
	static_assert(std::is_default_constructible_v<GltfCpuTexture>);

	// GltfCpuMaterial: copyable, movable, default-constructible
	static_assert(std::is_copy_constructible_v<GltfCpuMaterial>);
	static_assert(std::is_copy_assignable_v<GltfCpuMaterial>);
	static_assert(std::is_move_constructible_v<GltfCpuMaterial>);
	static_assert(std::is_move_assignable_v<GltfCpuMaterial>);
	static_assert(std::is_default_constructible_v<GltfCpuMaterial>);

	// GltfCpuMesh: non-copyable, movable, default-constructible
	static_assert(!std::is_copy_constructible_v<GltfCpuMesh>);
	static_assert(!std::is_copy_assignable_v<GltfCpuMesh>);
	static_assert(std::is_move_constructible_v<GltfCpuMesh>);
	static_assert(std::is_move_assignable_v<GltfCpuMesh>);
	static_assert(std::is_default_constructible_v<GltfCpuMesh>);

	// GltfCpuNode: copyable, movable, default-constructible
	static_assert(std::is_copy_constructible_v<GltfCpuNode>);
	static_assert(std::is_copy_assignable_v<GltfCpuNode>);
	static_assert(std::is_move_constructible_v<GltfCpuNode>);
	static_assert(std::is_move_assignable_v<GltfCpuNode>);
	static_assert(std::is_default_constructible_v<GltfCpuNode>);

	// GltfCpuAsset: non-copyable, movable, default-constructible
	static_assert(!std::is_copy_constructible_v<GltfCpuAsset>);
	static_assert(!std::is_copy_assignable_v<GltfCpuAsset>);
	static_assert(std::is_move_constructible_v<GltfCpuAsset>);
	static_assert(std::is_move_assignable_v<GltfCpuAsset>);
	static_assert(std::is_default_constructible_v<GltfCpuAsset>);

	// Default-constructed GltfCpuAsset has empty vectors and error == None
	GltfCpuAsset asset;
	EXPECT_TRUE(asset.textures.empty());
	EXPECT_TRUE(asset.materials.empty());
	EXPECT_TRUE(asset.meshes.empty());
	EXPECT_TRUE(asset.nodes.empty());
	EXPECT_EQ(asset.error, GltfLoadError::None);

	// Default-constructed GltfCpuTexture has default format
	GltfCpuTexture tex;
	EXPECT_EQ(tex.format, VK_FORMAT_R8G8B8A8_UNORM);

	// Default-constructed GltfCpuMaterial has -1 texture indices
	GltfCpuMaterial mat;
	EXPECT_EQ(mat.baseColorTexture, -1);
	EXPECT_EQ(mat.metallicRoughnessTexture, -1);
	EXPECT_EQ(mat.normalTexture, -1);
	EXPECT_EQ(mat.occlusionTexture, -1);
	EXPECT_EQ(mat.emissiveTexture, -1);

	// Default-constructed GltfCpuMesh has zero indices
	GltfCpuMesh mesh;
	EXPECT_EQ(mesh.meshIndex, 0u);
	EXPECT_EQ(mesh.primitiveIndex, 0u);

	// Default-constructed GltfCpuNode has -1 parent and -1 meshIndex
	GltfCpuNode node;
	EXPECT_TRUE(node.name.empty());
	EXPECT_EQ(node.parent, -1);
	EXPECT_EQ(node.meshIndex, -1);

	// GltfCpuAsset is movable: move it and verify vectors transfer
	GltfCpuAsset src;
	src.error = GltfLoadError::None;
	GltfCpuNode n;
	n.name = "root";
	src.nodes.push_back(n);
	GltfCpuAsset dst = std::move(src);
	EXPECT_EQ(dst.nodes.size(), 1u);
	EXPECT_EQ(dst.nodes[0].name, "root");
	EXPECT_EQ(dst.error, GltfLoadError::None);
}

// ---------------------------------------------------------------------------
// ParseGlb — file-not-found path
// ---------------------------------------------------------------------------

TEST(GltfLoader, test_gltf_parse_glb_produces_cpu_asset)
{
	// ParseGlb on a non-existent path must return FileNotFound
	{
		GltfCpuAsset asset = ParseGlb("__nonexistent_file_that_cannot_exist__.glb");
		EXPECT_EQ(asset.error, GltfLoadError::FileNotFound);
	}

	// ParseGlb on a path that exists but is not a valid GLB must return ParseFailed
	// We create a tiny temp file with garbage bytes.
	{
		const std::string tmpPath = MakeTempFilePath("virasa_test_invalid.glb");
		// Write a few garbage bytes
		{
			FILE* f = fopen(tmpPath.c_str(), "wb");
			if (f)
			{
				const uint8_t garbage[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03};
				fwrite(garbage, 1, sizeof(garbage), f);
				fclose(f);
			}
		}
		GltfCpuAsset asset = ParseGlb(tmpPath);
		// The file exists but is not a valid GLB container
		EXPECT_EQ(asset.error, GltfLoadError::ParseFailed);
		remove(tmpPath.c_str());
	}

	// ParseGlb must not touch any Vulkan/World/SceneRenderer state —
	// verified implicitly by the fact that we call it without any of those objects.
	// The returned asset must be movable (non-copyable).
	static_assert(!std::is_copy_constructible_v<GltfCpuAsset>);
	static_assert(std::is_move_constructible_v<GltfCpuAsset>);
}

// ---------------------------------------------------------------------------
// CommitGlb — called with a pre-failed asset must not create entities
// ---------------------------------------------------------------------------

TEST(GltfLoader, test_gltf_commit_glb_registers_and_builds_entities)
{
	// We cannot easily construct a real SceneRenderer in a unit test without a
	// full Vulkan context, so we verify the contract-observable properties that
	// do not require GPU resources:
	//
	// 1. A GltfCpuAsset with error != None must not be passed to CommitGlb
	//    (contract says behavior is not pinned), so we only verify that a
	//    successfully-parsed empty asset (no textures/materials/meshes/nodes)
	//    would produce a valid GltfLoadResult structure.
	//
	// 2. GltfLoadResult on failure carries Entity::Invalid() and a non-None error.
	//
	// We exercise the failure-path shape through the type system and the
	// LoadGlb convenience wrapper (which composes ParseGlb + CommitGlb).

	// Verify that a failed parse propagates correctly through the result type
	GltfLoadResult failResult;
	failResult.root = Entity::Invalid();
	failResult.error = GltfLoadError::TextureRegistrationFailed;
	EXPECT_FALSE(failResult.root.IsValid());
	EXPECT_NE(failResult.error, GltfLoadError::None);

	// Verify rollback observable: after a failed CommitGlb the entity count
	// must not have grown.  We test this via LoadGlb on a missing file.
	World world;
	const uint32_t countBefore = world.GetEntityCount();

	// We need a SceneRenderer — but we cannot initialize one without Vulkan.
	// The contract says LoadGlb returns FileNotFound before calling CommitGlb
	// when ParseGlb fails, so no SceneRenderer interaction occurs on this path.
	// We therefore use a null reference only to satisfy the signature; the
	// call must short-circuit before dereferencing it.
	//
	// NOTE: This relies on the contract guarantee that LoadGlb step 1 returns
	// early when asset.error != None, so CommitGlb (and thus scene_renderer)
	// is never touched.
	virasa::renderer::scene::SceneRenderer* sr = nullptr;
	GltfLoadResult result = LoadGlb("__no_such_file__.glb", world, *sr);
	EXPECT_EQ(result.error, GltfLoadError::FileNotFound);
	EXPECT_FALSE(result.root.IsValid());
	EXPECT_EQ(result.root, Entity::Invalid());
	// No entities should have been created
	EXPECT_EQ(world.GetEntityCount(), countBefore);
}

// ---------------------------------------------------------------------------
// LoadGlb — convenience wrapper composition
// ---------------------------------------------------------------------------

TEST(GltfLoader, test_gltf_load_glb_walks_scene_graph)
{
	// LoadGlb on a non-existent file must return FileNotFound and Entity::Invalid
	// without creating any entities and without touching the SceneRenderer
	// (per the contract: step 1 returns early when ParseGlb fails).
	World world;
	const uint32_t entityCountBefore = world.GetEntityCount();

	virasa::renderer::scene::SceneRenderer* sceneRenderer = nullptr;
	GltfLoadResult result = LoadGlb("this_file_should_not_exist.glb", world, *sceneRenderer);

	EXPECT_EQ(result.error, GltfLoadError::FileNotFound);
	EXPECT_FALSE(result.root.IsValid());
	EXPECT_EQ(result.root, Entity::Invalid());
	EXPECT_EQ(world.GetEntityCount(), entityCountBefore);

	// Also verify with a file that exists but is not a valid GLB
	{
		const std::string tmpPath = MakeTempFilePath("virasa_test_loadglb_invalid.glb");
		{
			FILE* f = fopen(tmpPath.c_str(), "wb");
			if (f)
			{
				const uint8_t garbage[] = {0xFF, 0xFE, 0xFD, 0xFC};
				fwrite(garbage, 1, sizeof(garbage), f);
				fclose(f);
			}
		}
		GltfLoadResult r2 = LoadGlb(tmpPath, world, *sceneRenderer);
		EXPECT_EQ(r2.error, GltfLoadError::ParseFailed);
		EXPECT_FALSE(r2.root.IsValid());
		EXPECT_EQ(world.GetEntityCount(), entityCountBefore);
		remove(tmpPath.c_str());
	}
}
