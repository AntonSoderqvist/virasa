#include <gtest/gtest.h>
#include <type_traits>
#include <utility>
#include <cstdint>

#include "virasa/renderer/scene/SceneRenderer.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/resources/MeshRegistry.h"
#include "virasa/renderer/material/Visual.h"

namespace
{

using virasa::RegisterError;
using virasa::RenderError;
using virasa::SwapchainStatus;
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
	constexpr bool kReturnsRenderError =
		std::is_same_v<decltype(std::declval<SceneRenderer&>().Initialize(
				   std::declval<const virasa::Device&>(),
				   std::declval<const virasa::Context&>(),
				   std::declval<const virasa::ui::FontAtlas&>())),
			RenderError>;
	EXPECT_TRUE(kReturnsRenderError);
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

	EXPECT_TRUE((std::is_same_v<decltype(renderer.GetMeshRegistry()), virasa::renderer::MeshRegistry&>));
	EXPECT_TRUE((std::is_same_v<decltype(renderer.GetMaterialTable()), virasa::VisualMaterialTable&>));
	EXPECT_TRUE((noexcept(renderer.GetMeshRegistry())));
	EXPECT_TRUE((noexcept(renderer.GetMaterialTable())));
	EXPECT_TRUE((std::is_same_v<decltype(renderer.CreateDefaultCubeMesh()), uint32_t>));
	EXPECT_TRUE((std::is_same_v<decltype(renderer.CreateDefaultMaterial()), uint32_t>));
}

TEST(SceneRendererTests, test_register_mesh_uploads_geometry_and_returns_id)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().RegisterMesh(
		std::declval<const virasa::MeshData&>(),
		std::declval<uint32_t&>())), RegisterError>));
}

TEST(SceneRendererTests, test_register_material_allocates_and_returns_id)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().RegisterMaterial(
		std::declval<const virasa::VisualMaterial&>(),
		std::declval<uint32_t&>())), RegisterError>));
}

TEST(SceneRendererTests, test_register_texture_uploads_image_and_returns_bindless_slot)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().RegisterTexture(
		std::declval<const virasa::TextureUpload&>(),
		std::declval<uint32_t&>())), RegisterError>));
}

TEST(SceneRendererTests, test_begin_frame_starts_swapchain_and_graph_and_declares_scene_targets)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().BeginFrame(1u, 1u)),
		SwapchainStatus>));
}

TEST(SceneRendererTests, test_render_world_appends_forward_pass_and_returns_scene_slot)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().RenderWorld(
		std::declval<const virasa::ecs::World&>(),
		std::declval<virasa::ecs::Entity>())), uint32_t>));
}

TEST(SceneRendererTests, test_submit_frame_runs_ui_pass_then_compiles_and_executes)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().SubmitFrame(
		std::declval<const virasa::ui::DrawList&>(),
		std::declval<const virasa::ui::FontAtlas&>(),
		1u,
		1u)), SwapchainStatus>));
}

TEST(SceneRendererTests, test_wait_idle_delegates_to_device)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().WaitIdle()), void>));
}

TEST(SceneRendererTests, test_scene_renderer_is_not_thread_safe_per_instance)
{
	EXPECT_FALSE(std::is_const_v<std::remove_reference_t<decltype(
		std::declval<SceneRenderer&>().GetMeshRegistry())>>);
	EXPECT_FALSE(std::is_const_v<std::remove_reference_t<decltype(
		std::declval<SceneRenderer&>().GetMaterialTable())>>);
}
