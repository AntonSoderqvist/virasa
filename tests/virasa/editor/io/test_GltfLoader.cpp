#include <gtest/gtest.h>

#include "virasa/editor/io/GltfLoader.h"

#include "virasa/ecs/Types.h"

#include <cstdint>
#include <type_traits>

namespace
{

using namespace virasa;
using namespace virasa::editor::io;

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
	static_assert(std::is_standard_layout_v<GltfLoadResult>);
	static_assert(offsetof(GltfLoadResult, root) == 0);
	static_assert(offsetof(GltfLoadResult, error) == sizeof(ecs::Entity));

	GltfLoadResult result;
	EXPECT_EQ(result.root, ecs::Entity::Invalid());
	EXPECT_FALSE(result.root.IsValid());
	EXPECT_EQ(result.error, GltfLoadError::None);

	GltfLoadResult assigned;
	assigned.root = ecs::Entity{42u, 7u};
	assigned.error = GltfLoadError::MeshRegistrationFailed;
	EXPECT_EQ(assigned.root.index, 42u);
	EXPECT_EQ(assigned.root.generation, 7u);
	EXPECT_EQ(assigned.error, GltfLoadError::MeshRegistrationFailed);

	GltfLoadResult copied = assigned;
	EXPECT_EQ(copied.root, assigned.root);
	EXPECT_EQ(copied.error, assigned.error);

	GltfLoadResult moved = std::move(copied);
	EXPECT_EQ(moved.root, assigned.root);
	EXPECT_EQ(moved.error, assigned.error);
}

TEST(GltfLoader, test_gltf_load_glb_walks_scene_graph)
{
	World world;
	const uint32_t entityCountBefore = world.GetEntityCount();

	SceneRenderer* sceneRenderer = nullptr;
	ASSERT_DEATH(
		{
			auto result = LoadGlb("this_file_should_not_exist.glb", world, *sceneRenderer);
			(void)result;
		},
		".*");

	EXPECT_EQ(world.GetEntityCount(), entityCountBefore);
}
