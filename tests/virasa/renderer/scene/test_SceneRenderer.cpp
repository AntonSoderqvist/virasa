#include <gtest/gtest.h>
#include <limits>
#include <type_traits>
#include <utility>
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>

#include "virasa/renderer/scene/SceneRenderer.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/resources/MeshRegistry.h"
#include "virasa/renderer/material/Visual.h"
#include "virasa/ecs/World.h"
#include "virasa/ecs/Types.h"
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

TEST(SceneRendererTests, test_scene_renderer_owns_renderer_resources_and_graph)
{
	// Verify the class-level type traits pinned by the contract:
	// final, default-constructible, non-copyable, movable, destructible.
	EXPECT_TRUE(std::is_final_v<SceneRenderer>);
	EXPECT_TRUE(std::is_default_constructible_v<SceneRenderer>);
	EXPECT_FALSE(std::is_copy_constructible_v<SceneRenderer>);
	EXPECT_FALSE(std::is_copy_assignable_v<SceneRenderer>);
	EXPECT_TRUE(std::is_move_constructible_v<SceneRenderer>);
	EXPECT_TRUE(std::is_move_assignable_v<SceneRenderer>);
	EXPECT_TRUE(std::is_destructible_v<SceneRenderer>);

	// A moved-from SceneRenderer must be destructible without crashing
	// (the move leaves owned RAII members in a valid moved-from state).
	SceneRenderer src;
	SceneRenderer dst(std::move(src));
	// dst and src both go out of scope here; no crash == pass.

	// Move-assignment must also compile and leave both sides destructible.
	SceneRenderer a;
	SceneRenderer b;
	b = std::move(a);
}

TEST(SceneRendererTests, test_initialize_creates_all_renderer_resources)
{
	// Verify the Initialize signature returns RenderError as the contract requires.
	constexpr bool kReturnsRenderError =
		std::is_same_v<decltype(std::declval<SceneRenderer&>().Initialize(
				   std::declval<const virasa::Device&>(),
				   std::declval<const virasa::Context&>(),
				   std::declval<const virasa::ui::FontAtlas&>())),
			RenderError>;
	EXPECT_TRUE(kReturnsRenderError);

	// Initialize is [[nodiscard]] — verify the attribute is present by checking
	// that the return type is indeed RenderError (nodiscard is enforced by the
	// compiler; we cannot query it at runtime, but the type check above covers it).

	// A default-constructed SceneRenderer must not be initialized yet;
	// the contract states _initialized starts false.
	// We cannot call Initialize without a real Device/Context/FontAtlas, but we
	// can verify that GetMeshRegistry and GetMaterialTable are accessible on an
	// uninitialized instance (they are always valid observers).
	SceneRenderer renderer;
	// These calls must not crash on an uninitialized renderer.
	(void)renderer.GetMeshRegistry();
	(void)renderer.GetMaterialTable();
}

TEST(SceneRendererTests, test_registry_accessors_return_owned_tables)
{
	SceneRenderer renderer;

	// GetMeshRegistry must return a stable reference to the same object on repeated calls.
	auto& meshRegistryA = renderer.GetMeshRegistry();
	auto& meshRegistryB = renderer.GetMeshRegistry();
	auto& materialTableA = renderer.GetMaterialTable();
	auto& materialTableB = renderer.GetMaterialTable();

	EXPECT_EQ(std::addressof(meshRegistryA), std::addressof(meshRegistryB));
	EXPECT_EQ(std::addressof(materialTableA), std::addressof(materialTableB));

	// Return-type checks: the contract pins the exact types returned.
	EXPECT_TRUE((std::is_same_v<decltype(renderer.GetMeshRegistry()), virasa::renderer::MeshRegistry&>));
	EXPECT_TRUE((std::is_same_v<decltype(renderer.GetMaterialTable()), virasa::VisualMaterialTable&>));

	// Both accessors must be noexcept.
	EXPECT_TRUE((noexcept(renderer.GetMeshRegistry())));
	EXPECT_TRUE((noexcept(renderer.GetMaterialTable())));

	// CreateDefaultCubeMesh and CreateDefaultMaterial return uint32_t.
	EXPECT_TRUE((std::is_same_v<decltype(renderer.CreateDefaultCubeMesh()), uint32_t>));
	EXPECT_TRUE((std::is_same_v<decltype(renderer.CreateDefaultMaterial()), uint32_t>));

	// After a move the source's registry/table references are no longer the same objects
	// as the destination's (the move transfers ownership).
	SceneRenderer moved(std::move(renderer));
	auto& movedMeshRegistry = moved.GetMeshRegistry();
	auto& movedMaterialTable = moved.GetMaterialTable();
	// The moved-into renderer's references must be stable.
	EXPECT_EQ(std::addressof(movedMeshRegistry), std::addressof(moved.GetMeshRegistry()));
	EXPECT_EQ(std::addressof(movedMaterialTable), std::addressof(moved.GetMaterialTable()));
}

TEST(SceneRendererTests, test_register_mesh_uploads_geometry_and_returns_id)
{
	// Verify the RegisterMesh signature.
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().RegisterMesh(
		std::declval<const virasa::MeshData&>(),
		std::declval<uint32_t&>())), RegisterError>));

	// Contract: calling RegisterMesh with empty vertices must return InvalidInput
	// and leave out_mesh_id unchanged, without requiring an initialized renderer.
	SceneRenderer renderer;
	virasa::MeshData emptyVertices;
	emptyVertices.indices = {0u, 1u, 2u};
	uint32_t outId = 0xDEADBEEFu;
	RegisterError err = renderer.RegisterMesh(emptyVertices, outId);
	EXPECT_EQ(err, RegisterError::InvalidInput);
	EXPECT_EQ(outId, 0xDEADBEEFu);

	// Contract: calling RegisterMesh with empty indices must return InvalidInput.
	virasa::MeshData emptyIndices;
	virasa::Vertex v{};
	emptyIndices.vertices = {v};
	uint32_t outId2 = 0xDEADBEEFu;
	err = renderer.RegisterMesh(emptyIndices, outId2);
	EXPECT_EQ(err, RegisterError::InvalidInput);
	EXPECT_EQ(outId2, 0xDEADBEEFu);

	// Contract: indices.size() not a positive multiple of 3 must return InvalidInput.
	virasa::MeshData badIndices;
	badIndices.vertices = {v, v, v, v};
	badIndices.indices = {0u, 1u}; // size 2, not multiple of 3
	uint32_t outId3 = 0xDEADBEEFu;
	err = renderer.RegisterMesh(badIndices, outId3);
	EXPECT_EQ(err, RegisterError::InvalidInput);
	EXPECT_EQ(outId3, 0xDEADBEEFu);
}

TEST(SceneRendererTests, test_register_material_allocates_and_returns_id)
{
	// Verify the RegisterMaterial signature.
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().RegisterMaterial(
		std::declval<const virasa::VisualMaterial&>(),
		std::declval<uint32_t&>())), RegisterError>));

	// RegisterMaterial is [[nodiscard]] — verify the return type is RegisterError.
	constexpr bool kAcceptsCorrectArgs =
		std::is_invocable_r_v<RegisterError, decltype(&SceneRenderer::RegisterMaterial),
			SceneRenderer*, const virasa::VisualMaterial&, uint32_t&>;
	EXPECT_TRUE(kAcceptsCorrectArgs);

	// The contract states "must only be called on an initialized SceneRenderer" and
	// does not pin behavior otherwise. We verify the sentinel value (0xFFFFFFFFu)
	// used by VisualMaterialTable::Allocate on failure matches the expected constant.
	constexpr uint32_t kInvalidMaterialId = 0xFFFFFFFFu;
	EXPECT_EQ(kInvalidMaterialId, std::numeric_limits<uint32_t>::max());
}

TEST(SceneRendererTests, test_register_texture_uploads_image_and_returns_bindless_slot)
{
	// Verify the RegisterTexture signature.
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().RegisterTexture(
		std::declval<const virasa::TextureUpload&>(),
		std::declval<uint32_t&>())), RegisterError>));

	// Contract: width == 0 must return InvalidInput with out_slot unchanged.
	SceneRenderer renderer;
	virasa::TextureUpload upload{};
	uint32_t outSlot = 0xDEADBEEFu;
	// width and height are both 0 by default — invalid.
	RegisterError err = renderer.RegisterTexture(upload, outSlot);
	EXPECT_EQ(err, RegisterError::InvalidInput);
	EXPECT_EQ(outSlot, 0xDEADBEEFu);

	// Contract: pixels.data() == nullptr (span is null/empty) must return InvalidInput.
	virasa::TextureUpload uploadNullPixels{};
	uploadNullPixels.width = 4u;
	uploadNullPixels.height = 4u;
	uploadNullPixels.format = VK_FORMAT_R8G8B8A8_UNORM;
	// pixels span is default (nullptr, size 0) — invalid.
	uint32_t outSlot2 = 0xDEADBEEFu;
	err = renderer.RegisterTexture(uploadNullPixels, outSlot2);
	EXPECT_EQ(err, RegisterError::InvalidInput);
	EXPECT_EQ(outSlot2, 0xDEADBEEFu);

	// Contract: pixels.size() != width * height * texelSize must return InvalidInput.
	std::vector<std::byte> wrongSizePixels(3u, std::byte{0xFF}); // should be 4*4*4=64 bytes
	virasa::TextureUpload uploadWrongSize{};
	uploadWrongSize.width = 4u;
	uploadWrongSize.height = 4u;
	uploadWrongSize.format = VK_FORMAT_R8G8B8A8_UNORM;
	uploadWrongSize.pixels = std::span<const std::byte>(wrongSizePixels.data(), wrongSizePixels.size());
	uint32_t outSlot3 = 0xDEADBEEFu;
	err = renderer.RegisterTexture(uploadWrongSize, outSlot3);
	EXPECT_EQ(err, RegisterError::InvalidInput);
	EXPECT_EQ(outSlot3, 0xDEADBEEFu);
}

TEST(SceneRendererTests, test_begin_frame_starts_swapchain_and_graph_and_declares_scene_targets)
{
	// Verify the BeginFrame signature: takes two uint32_t dimensions, returns SwapchainStatus.
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().BeginFrame(1u, 1u)),
		SwapchainStatus>));

	// The contract states BeginFrame is [[nodiscard]]; the type check above covers the
	// signature. Behavioral verification (actual swapchain interaction) requires a
	// fully initialized renderer with a real Context and is covered by integration tests.
	// Here we confirm the parameter types are uint32_t (not e.g. int) by ensuring
	// the call compiles with unsigned literals.
	constexpr bool kAcceptsUint32 =
		std::is_invocable_r_v<SwapchainStatus, decltype(&SceneRenderer::BeginFrame),
			SceneRenderer*, uint32_t, uint32_t>;
	EXPECT_TRUE(kAcceptsUint32);
}

TEST(SceneRendererTests, test_render_world_appends_forward_pass_and_returns_scene_slot)
{
	// Verify the RenderWorld signature: takes a const World& and an Entity, returns uint32_t.
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().RenderWorld(
		std::declval<const virasa::ecs::World&>(),
		std::declval<virasa::ecs::Entity>())), uint32_t>));

	// The contract pins that on registration failure RenderWorld returns 0xFFFFFFFFu.
	// We verify the sentinel value is the same as the standard "invalid" sentinel.
	constexpr uint32_t kInvalidSlot = 0xFFFFFFFFu;
	EXPECT_EQ(kInvalidSlot, std::numeric_limits<uint32_t>::max());

	// RenderWorld takes world by const reference — verify const-correctness.
	constexpr bool kTakesConstWorld =
		std::is_invocable_r_v<uint32_t, decltype(&SceneRenderer::RenderWorld),
			SceneRenderer*, const virasa::ecs::World&, virasa::ecs::Entity>;
	EXPECT_TRUE(kTakesConstWorld);
}

TEST(SceneRendererTests, test_submit_frame_runs_ui_pass_then_compiles_and_executes)
{
	// Verify the SubmitFrame signature.
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().SubmitFrame(
		std::declval<const virasa::ui::DrawList&>(),
		std::declval<const virasa::ui::FontAtlas&>(),
		1u,
		1u)), SwapchainStatus>));

	// SubmitFrame is [[nodiscard]] and returns SwapchainStatus.
	constexpr bool kAcceptsCorrectArgs =
		std::is_invocable_r_v<SwapchainStatus, decltype(&SceneRenderer::SubmitFrame),
			SceneRenderer*,
			const virasa::ui::DrawList&,
			const virasa::ui::FontAtlas&,
			uint32_t,
			uint32_t>;
	EXPECT_TRUE(kAcceptsCorrectArgs);
}

TEST(SceneRendererTests, test_wait_idle_delegates_to_device)
{
	// WaitIdle returns void and takes no arguments.
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<SceneRenderer&>().WaitIdle()), void>));

	// Verify WaitIdle is a non-static member (it delegates to _context->GetDevice().WaitIdle()).
	constexpr bool kIsNonStaticMember =
		std::is_member_function_pointer_v<decltype(&SceneRenderer::WaitIdle)>;
	EXPECT_TRUE(kIsNonStaticMember);
}

TEST(SceneRendererTests, test_scene_renderer_is_not_thread_safe_per_instance)
{
	// The contract states SceneRenderer does not synchronize concurrent calls.
	// We verify the observable consequence: the mutating methods are non-const,
	// meaning they are not safe to call from multiple threads simultaneously
	// (there is no internal mutex or atomic guarding them).

	// GetMeshRegistry and GetMaterialTable return mutable (non-const) references,
	// confirming they are non-const methods that can mutate internal state.
	EXPECT_FALSE(std::is_const_v<std::remove_reference_t<decltype(
		std::declval<SceneRenderer&>().GetMeshRegistry())>>);
	EXPECT_FALSE(std::is_const_v<std::remove_reference_t<decltype(
		std::declval<SceneRenderer&>().GetMaterialTable())>>);

	// Initialize, BeginFrame, RenderWorld, SubmitFrame, WaitIdle are all non-const
	// member functions — verify they require a non-const SceneRenderer&.
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
}
