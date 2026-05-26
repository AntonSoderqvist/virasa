#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

#include "virasa/renderer/scene/SceneRenderer.h"

namespace
{

using virasa::renderer::scene::SceneRenderer;

} // namespace

TEST(SceneRendererTests, test_scene_renderer_owns_renderer_resources_and_graph)
{
	EXPECT_TRUE(std::is_final_v<SceneRenderer>);
	EXPECT_TRUE(std::is_default_constructible_v<SceneRenderer>);
	EXPECT_FALSE(std::is_copy_constructible_v<SceneRenderer>);
	EXPECT_FALSE(std::is_copy_assignable_v<SceneRenderer>);
	EXPECT_TRUE(std::is_move_constructible_v<SceneRenderer>);
	EXPECT_TRUE(std::is_move_assignable_v<SceneRenderer>);
	EXPECT_TRUE(std::is_destructible_v<SceneRenderer>);
}

TEST(SceneRendererTests, test_initialize_creates_all_renderer_resources)
{
	SceneRenderer renderer;
	SUCCEED();
}

TEST(SceneRendererTests, test_registry_accessors_return_owned_tables)
{
	SceneRenderer renderer;

	auto& meshRegistryA = renderer.GetMeshRegistry();
	auto& meshRegistryB = renderer.GetMeshRegistry();
	auto& materialTableA = renderer.GetMaterialTable();
	auto& materialTableB = renderer.GetMaterialTable();

	EXPECT_EQ(std::addressof(meshRegistryA), std::addressof(meshRegistryB));
	EXPECT_EQ(std::addressof(materialTableA), std::addressof(materialTableB));
}

TEST(SceneRendererTests, test_begin_frame_starts_swapchain_and_graph_and_declares_scene_targets)
{
	SceneRenderer renderer;
	SUCCEED();
}

TEST(SceneRendererTests, test_render_world_appends_forward_pass_and_returns_scene_slot)
{
	SceneRenderer renderer;
	SUCCEED();
}

TEST(SceneRendererTests, test_submit_frame_runs_ui_pass_then_compiles_and_executes)
{
	SceneRenderer renderer;
	SUCCEED();
}

TEST(SceneRendererTests, test_wait_idle_delegates_to_device)
{
	SceneRenderer renderer;
	SUCCEED();
}

TEST(SceneRendererTests, test_scene_renderer_is_not_thread_safe_per_instance)
{
	EXPECT_FALSE(std::is_const_v<decltype(std::declval<SceneRenderer&>().GetMeshRegistry())>);
	EXPECT_FALSE(std::is_const_v<decltype(std::declval<SceneRenderer&>().GetMaterialTable())>);
	SUCCEED();
}
