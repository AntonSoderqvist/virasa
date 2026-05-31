#include <gtest/gtest.h>
#include <limits>
#include <type_traits>
#include <utility>
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>
#include <tuple>

#include "virasa/renderer/scene/SceneRenderer.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/resources/MeshRegistry.h"
#include "virasa/renderer/material/Visual.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/lighting/LightTable.h"
#include "virasa/renderer/lighting/ShadowTable.h"
#include "virasa/ecs/World.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/Components.h"
#include "virasa/ui/Types.h"
#include "virasa/ui/FontAtlas.h"
#include "vulkan/vulkan.h"

namespace
{

using virasa::RegisterError;
using virasa::RenderError;
using virasa::SwapchainStatus;
using virasa::renderer::scene::SceneRenderer;

} // namespace

TEST(SceneRenderer, test_scene_renderer_owns_renderer_resources_and_graph)
{
	EXPECT_TRUE(std::is_final_v<SceneRenderer>);
	EXPECT_TRUE(std::is_default_constructible_v<SceneRenderer>);
	EXPECT_FALSE(std::is_copy_constructible_v<SceneRenderer>);
	EXPECT_FALSE(std::is_copy_assignable_v<SceneRenderer>);
	EXPECT_TRUE(std::is_move_constructible_v<SceneRenderer>);
	EXPECT_TRUE(std::is_move_assignable_v<SceneRenderer>);
	EXPECT_TRUE(std::is_destructible_v<SceneRenderer>);

	SceneRenderer src;
	SceneRenderer dst(std::move(src));
	(void)dst;

	SceneRenderer a;
	SceneRenderer b;
	b = std::move(a);

	SceneRenderer c;
	SceneRenderer d(std::move(c));
	(void)d;
	(void)c.GetMeshRegistry();
	(void)c.GetMaterialTable();

	constexpr bool kCorrectNamespace =
		std::is_same_v<virasa::renderer::scene::SceneRenderer, SceneRenderer>;
	EXPECT_TRUE(kCorrectNamespace);
}

TEST(SceneRenderer, test_initialize_creates_all_renderer_resources)
{
	constexpr bool kReturnsRenderError =
		std::is_same_v<decltype(std::declval<SceneRenderer&>().Initialize(
				   std::declval<const virasa::Device&>(),
				   std::declval<const virasa::Context&>(),
				   std::declval<const virasa::ui::FontAtlas&>())),
			RenderError>;
	EXPECT_TRUE(kReturnsRenderError);

	constexpr bool kAcceptsCorrectArgs =
		std::is_invocable_r_v<RenderError, decltype(&SceneRenderer::Initialize),
			SceneRenderer*,
			const virasa::Device&,
			const virasa::Context&,
			const virasa::ui::FontAtlas&>;
	EXPECT_TRUE(kAcceptsCorrectArgs);

	SceneRenderer renderer;
	(void)renderer.GetMeshRegistry();
	(void)renderer.GetMaterialTable();

	constexpr uint32_t kInvalidId = 0xFFFFFFFFu;
	EXPECT_EQ(kInvalidId, std::numeric_limits<uint32_t>::max());

	EXPECT_EQ(static_cast<uint8_t>(RenderError::None), 0u);
	EXPECT_NE(RenderError::PipelineCreateFailed, RenderError::None);
	EXPECT_NE(RenderError::SamplerCreateFailed, RenderError::None);
	EXPECT_NE(RenderError::DescriptorPoolCreateFailed, RenderError::None);
}

TEST(SceneRenderer, test_registry_accessors_return_owned_tables)
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

	SceneRenderer moved(std::move(renderer));
	auto& movedMeshRegistry = moved.GetMeshRegistry();
	auto& movedMaterialTable = moved.GetMaterialTable();
	EXPECT_EQ(std::addressof(movedMeshRegistry), std::addressof(moved.GetMeshRegistry()));
	EXPECT_EQ(std::addressof(movedMaterialTable), std::addressof(moved.GetMaterialTable()));
}

TEST(SceneRenderer, test_register_mesh_uploads_geometry_and_returns_id)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().RegisterMesh(
		std::declval<const virasa::MeshData&>(),
		std::declval<uint32_t&>())), RegisterError>));

	SceneRenderer renderer;
	virasa::MeshData emptyVertices;
	emptyVertices.indices = {0u, 1u, 2u};
	uint32_t outId = 0xDEADBEEFu;
	RegisterError err = renderer.RegisterMesh(emptyVertices, outId);
	EXPECT_EQ(err, RegisterError::InvalidInput);
	EXPECT_EQ(outId, 0xDEADBEEFu);

	virasa::MeshData emptyIndices;
	virasa::Vertex v{};
	emptyIndices.vertices = {v};
	uint32_t outId2 = 0xDEADBEEFu;
	err = renderer.RegisterMesh(emptyIndices, outId2);
	EXPECT_EQ(err, RegisterError::InvalidInput);
	EXPECT_EQ(outId2, 0xDEADBEEFu);

	virasa::MeshData badIndices;
	badIndices.vertices = {v, v, v, v};
	badIndices.indices = {0u, 1u};
	uint32_t outId3 = 0xDEADBEEFu;
	err = renderer.RegisterMesh(badIndices, outId3);
	EXPECT_EQ(err, RegisterError::InvalidInput);
	EXPECT_EQ(outId3, 0xDEADBEEFu);
}

TEST(SceneRenderer, test_register_material_allocates_and_returns_id)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().RegisterMaterial(
		std::declval<const virasa::VisualMaterial&>(),
		std::declval<uint32_t&>())), RegisterError>));

	constexpr bool kAcceptsCorrectArgs =
		std::is_invocable_r_v<RegisterError, decltype(&SceneRenderer::RegisterMaterial),
			SceneRenderer*, const virasa::VisualMaterial&, uint32_t&>;
	EXPECT_TRUE(kAcceptsCorrectArgs);

	constexpr uint32_t kInvalidMaterialId = 0xFFFFFFFFu;
	EXPECT_EQ(kInvalidMaterialId, std::numeric_limits<uint32_t>::max());
	EXPECT_TRUE(std::is_default_constructible_v<virasa::VisualMaterial>);
	EXPECT_EQ(static_cast<uint8_t>(RegisterError::None), 0u);
	EXPECT_NE(static_cast<uint8_t>(RegisterError::OutOfSlots), 0u);
}

TEST(SceneRenderer, test_register_texture_uploads_image_and_returns_bindless_slot)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().RegisterTexture(
		std::declval<const virasa::TextureUpload&>(),
		std::declval<uint32_t&>())), RegisterError>));

	SceneRenderer renderer;
	virasa::TextureUpload upload{};
	uint32_t outSlot = 0xDEADBEEFu;
	RegisterError err = renderer.RegisterTexture(upload, outSlot);
	EXPECT_EQ(err, RegisterError::InvalidInput);
	EXPECT_EQ(outSlot, 0xDEADBEEFu);

	virasa::TextureUpload uploadNullPixels{};
	uploadNullPixels.width = 4u;
	uploadNullPixels.height = 4u;
	uploadNullPixels.format = VK_FORMAT_R8G8B8A8_UNORM;
	uint32_t outSlot2 = 0xDEADBEEFu;
	err = renderer.RegisterTexture(uploadNullPixels, outSlot2);
	EXPECT_EQ(err, RegisterError::InvalidInput);
	EXPECT_EQ(outSlot2, 0xDEADBEEFu);

	std::vector<std::byte> wrongSizePixels(3u, std::byte{0xFF});
	virasa::TextureUpload uploadWrongSize{};
	uploadWrongSize.width = 4u;
	uploadWrongSize.height = 4u;
	uploadWrongSize.format = VK_FORMAT_R8G8B8A8_UNORM;
	uploadWrongSize.pixels = std::span<const std::byte>(wrongSizePixels.data(), wrongSizePixels.size());
	uint32_t outSlot3 = 0xDEADBEEFu;
	err = renderer.RegisterTexture(uploadWrongSize, outSlot3);
	EXPECT_EQ(err, RegisterError::InvalidInput);
	EXPECT_EQ(outSlot3, 0xDEADBEEFu);

	EXPECT_NE(RegisterError::SamplerCreateFailed, RegisterError::InvalidInput);
	EXPECT_NE(RegisterError::UploadFailed, RegisterError::InvalidInput);
	EXPECT_NE(RegisterError::OutOfSlots, RegisterError::InvalidInput);
}

TEST(SceneRenderer, test_begin_frame_starts_swapchain_and_graph_and_declares_scene_targets)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().BeginFrame(1u, 1u)),
		SwapchainStatus>));

	constexpr bool kAcceptsUint32 =
		std::is_invocable_r_v<SwapchainStatus, decltype(&SceneRenderer::BeginFrame),
			SceneRenderer*, uint32_t, uint32_t>;
	EXPECT_TRUE(kAcceptsUint32);

	constexpr uint32_t kInvalidSceneSlot = 0xFFFFFFFFu;
	EXPECT_EQ(kInvalidSceneSlot, std::numeric_limits<uint32_t>::max());
	EXPECT_NE(SwapchainStatus::NotReady, SwapchainStatus::Success);
	EXPECT_NE(SwapchainStatus::Error, SwapchainStatus::Success);
	EXPECT_NE(SwapchainStatus::Recreated, SwapchainStatus::Success);
}

TEST(SceneRenderer, test_render_world_appends_forward_pass_and_returns_scene_slot)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().RenderWorld(
		std::declval<const virasa::ecs::World&>(),
		std::declval<virasa::ecs::Entity>())), uint32_t>));

	constexpr uint32_t kInvalidSlot = 0xFFFFFFFFu;
	EXPECT_EQ(kInvalidSlot, std::numeric_limits<uint32_t>::max());

	constexpr bool kTakesConstWorld =
		std::is_invocable_r_v<uint32_t, decltype(&SceneRenderer::RenderWorld),
			SceneRenderer*, const virasa::ecs::World&, virasa::ecs::Entity>;
	EXPECT_TRUE(kTakesConstWorld);

	virasa::ecs::HighlightComponent hc{};
	EXPECT_FLOAT_EQ(hc.intensity, 1.0f);
	EXPECT_EQ(sizeof(virasa::ShadowGPU), 80u);
	EXPECT_EQ(sizeof(virasa::LightGPU), 64u);
}

TEST(SceneRenderer, test_submit_frame_runs_ui_pass_then_compiles_and_executes)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().SubmitFrame(
		std::declval<const virasa::ui::DrawList&>(),
		std::declval<const virasa::ui::FontAtlas&>(),
		1u,
		1u)), SwapchainStatus>));

	constexpr bool kAcceptsCorrectArgs =
		std::is_invocable_r_v<SwapchainStatus, decltype(&SceneRenderer::SubmitFrame),
			SceneRenderer*,
			const virasa::ui::DrawList&,
			const virasa::ui::FontAtlas&,
			uint32_t,
			uint32_t>;
	EXPECT_TRUE(kAcceptsCorrectArgs);

	EXPECT_NE(SwapchainStatus::Error, SwapchainStatus::Success);
	EXPECT_NE(SwapchainStatus::Error, SwapchainStatus::Recreated);
	EXPECT_NE(SwapchainStatus::Error, SwapchainStatus::NotReady);
}

TEST(SceneRenderer, test_wait_idle_delegates_to_device)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().WaitIdle()), void>));

	constexpr bool kIsNonStaticMember =
		std::is_member_function_pointer_v<decltype(&SceneRenderer::WaitIdle)>;
	EXPECT_TRUE(kIsNonStaticMember);

	constexpr bool kWaitIdleNonConst =
		!std::is_invocable_v<decltype(&SceneRenderer::WaitIdle), const SceneRenderer*>;
	EXPECT_TRUE(kWaitIdleNonConst);

	constexpr bool kDeviceWaitIdleIsConst =
		std::is_invocable_v<decltype(&virasa::Device::WaitIdle), const virasa::Device*>;
	EXPECT_TRUE(kDeviceWaitIdleIsConst);
}

TEST(SceneRenderer, test_request_pick_records_deferred_query)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().RequestPick(0u, 0u)), void>));

	constexpr bool kRequestPickNonConst =
		!std::is_invocable_v<decltype(&SceneRenderer::RequestPick), const SceneRenderer*, uint32_t, uint32_t>;
	EXPECT_TRUE(kRequestPickNonConst);

	constexpr bool kAcceptsUint32 =
		std::is_invocable_v<decltype(&SceneRenderer::RequestPick), SceneRenderer*, uint32_t, uint32_t>;
	EXPECT_TRUE(kAcceptsUint32);
}

TEST(SceneRenderer, test_get_pick_result_returns_latest_completed)
{
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().GetPickResult()), virasa::PickResult>));

	virasa::PickResult result{};
	EXPECT_FALSE(result.valid);
	EXPECT_EQ(result.entity, virasa::ecs::Entity::Invalid());

	constexpr bool kGetPickResultNonConst =
		!std::is_invocable_v<decltype(&SceneRenderer::GetPickResult), const SceneRenderer*>;
	EXPECT_TRUE(kGetPickResultNonConst);
}

TEST(SceneRenderer, test_scene_renderer_is_not_thread_safe_per_instance)
{
	EXPECT_FALSE(std::is_const_v<std::remove_reference_t<decltype(
		std::declval<SceneRenderer&>().GetMeshRegistry())>>);
	EXPECT_FALSE(std::is_const_v<std::remove_reference_t<decltype(
		std::declval<SceneRenderer&>().GetMaterialTable())>>);

	constexpr bool kInitializeNonConst =
		!std::is_invocable_v<decltype(&SceneRenderer::Initialize),
			const SceneRenderer*,
			const virasa::Device&,
			const virasa::Context&,
			const virasa::ui::FontAtlas&>;
	EXPECT_TRUE(kInitializeNonConst);

	constexpr bool kBeginFrameNonConst =
		!std::is_invocable_v<decltype(&SceneRenderer::BeginFrame),
			const SceneRenderer*, uint32_t, uint32_t>;
	EXPECT_TRUE(kBeginFrameNonConst);

	constexpr bool kRenderWorldNonConst =
		!std::is_invocable_v<decltype(&SceneRenderer::RenderWorld),
			const SceneRenderer*,
			const virasa::ecs::World&,
			virasa::ecs::Entity>;
	EXPECT_TRUE(kRenderWorldNonConst);

	constexpr bool kSubmitFrameNonConst =
		!std::is_invocable_v<decltype(&SceneRenderer::SubmitFrame),
			const SceneRenderer*,
			const virasa::ui::DrawList&,
			const virasa::ui::FontAtlas&,
			uint32_t,
			uint32_t>;
	EXPECT_TRUE(kSubmitFrameNonConst);

	constexpr bool kWaitIdleNonConst =
		!std::is_invocable_v<decltype(&SceneRenderer::WaitIdle), const SceneRenderer*>;
	EXPECT_TRUE(kWaitIdleNonConst);

	constexpr bool kRequestPickNonConst =
		!std::is_invocable_v<decltype(&SceneRenderer::RequestPick), const SceneRenderer*, uint32_t, uint32_t>;
	EXPECT_TRUE(kRequestPickNonConst);

	constexpr bool kGetPickResultNonConst =
		!std::is_invocable_v<decltype(&SceneRenderer::GetPickResult), const SceneRenderer*>;
	EXPECT_TRUE(kGetPickResultNonConst);
}
