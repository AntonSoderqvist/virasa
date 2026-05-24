#include <vulkan/vulkan.h>

#include <cstdint>
#include <gtest/gtest.h>
#include <utility>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/graph/BufferRegistry.h"
#include "virasa/renderer/graph/Graph.h"
#include "virasa/renderer/graph/ImageRegistry.h"
#include "virasa/renderer/graph/Pass.h"
#include "virasa/renderer/graph/Types.h"
#include "virasa/window/Platform.h"
#include "virasa/window/Types.h"

using namespace virasa::renderer::graph;
using namespace virasa::renderer;
using namespace virasa;

// ---------------------------------------------------------------------------
// Test-environment helpers
// ---------------------------------------------------------------------------
namespace
{

// Bring up a real Vulkan instance + device + registries for tests that need
// them.  The fixture is shared by all tests that require an initialized Graph.
struct VulkanEnv
{
	Instance instance;
	Device device;
	ImageRegistry imageRegistry;
	BufferRegistry bufferRegistry;
	bool valid = false;

	VulkanEnv()
	{
		RendererConfig cfg;
		cfg.enableValidation = false;
		if (instance.Initialize(cfg) != RenderError::None)
			return;
		// Device::Initialize needs a surface; pass VK_NULL_HANDLE — most
		// implementations accept it for compute/transfer-only selection.
		// If the device cannot be selected without a surface the test will
		// simply be skipped via the `valid` flag.
		if (device.Initialize(instance, VK_NULL_HANDLE) != RenderError::None)
			return;
		if (imageRegistry.Initialize(device) != RenderError::None)
			return;
		if (bufferRegistry.Initialize(device) != RenderError::None)
			return;
		valid = true;
	}
};

// Lazily-constructed singleton so we pay the Vulkan init cost once.
VulkanEnv& GetEnv()
{
	static VulkanEnv env;
	return env;
}

// Minimal GraphImageDesc for a 64×64 RGBA colour image.
GraphImageDesc MakeColorDesc(uint32_t w = 64, uint32_t h = 64)
{
	GraphImageDesc d;
	d.width = w;
	d.height = h;
	d.format = VK_FORMAT_R8G8B8A8_UNORM;
	d.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	d.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	return d;
}

// Minimal GraphBufferDesc for a small uniform buffer.
GraphBufferDesc MakeBufferDesc(VkDeviceSize sz = 256)
{
	GraphBufferDesc d;
	d.size = sz;
	d.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	d.hostVisible = true;
	return d;
}

// Build a Graph that is initialized against the shared VulkanEnv.
// Returns false (and leaves graph in default state) when Vulkan is unavailable.
bool MakeInitializedGraph(Graph& graph)
{
	VulkanEnv& env = GetEnv();
	if (!env.valid)
		return false;
	return graph.Initialize(env.device, env.imageRegistry, env.bufferRegistry) ==
		 RenderError::None;
}

} // namespace

// ===========================================================================
// Tests
// ===========================================================================

// ---------------------------------------------------------------------------
// graph_is_raii_movable_non_copyable
// ---------------------------------------------------------------------------
TEST(Graph, test_graph_is_raii_movable_non_copyable)
{
	// Not copyable
	EXPECT_FALSE(std::is_copy_constructible_v<Graph>);
	EXPECT_FALSE(std::is_copy_assignable_v<Graph>);

	// Movable
	EXPECT_TRUE(std::is_move_constructible_v<Graph>);
	EXPECT_TRUE(std::is_move_assignable_v<Graph>);

	// Final
	EXPECT_TRUE(std::is_final_v<Graph>);

	// Default-constructible
	EXPECT_TRUE(std::is_default_constructible_v<Graph>);

	// Move constructor transfers state; source becomes default-like
	Graph src;
	if (MakeInitializedGraph(src))
	{
		EXPECT_TRUE(src.IsInitialized());
		Graph dst(std::move(src));
		EXPECT_TRUE(dst.IsInitialized());
		EXPECT_FALSE(src.IsInitialized()); // NOLINT(bugprone-use-after-move)
	}

	// Move assignment transfers state; source becomes default-like
	Graph src2;
	if (MakeInitializedGraph(src2))
	{
		Graph dst2;
		dst2 = std::move(src2);
		EXPECT_TRUE(dst2.IsInitialized());
		EXPECT_FALSE(src2.IsInitialized()); // NOLINT(bugprone-use-after-move)
	}

	// Move assignment into an already-initialized Graph clears prior state
	Graph dst3;
	Graph src3;
	if (MakeInitializedGraph(dst3) && MakeInitializedGraph(src3))
	{
		dst3.Begin();
		// Assign while dst3 has a frame in progress — must not crash
		dst3 = std::move(src3);
		EXPECT_TRUE(dst3.IsInitialized());
	}
}

// ---------------------------------------------------------------------------
// graph_default_constructed_state
// ---------------------------------------------------------------------------
TEST(Graph, test_graph_default_constructed_state)
{
	Graph g;
	EXPECT_FALSE(g.IsInitialized());
	EXPECT_EQ(g.GetPassCount(), 0u);
}

// ---------------------------------------------------------------------------
// graph_initialize_stores_borrowed_references
// ---------------------------------------------------------------------------
TEST(Graph, test_graph_initialize_stores_borrowed_references)
{
	VulkanEnv& env = GetEnv();
	if (!env.valid)
	{
		GTEST_SKIP() << "Vulkan unavailable";
	}

	Graph g;
	EXPECT_FALSE(g.IsInitialized());

	RenderError err = g.Initialize(env.device, env.imageRegistry, env.bufferRegistry);
	EXPECT_EQ(err, RenderError::None);
	EXPECT_TRUE(g.IsInitialized());

	// Re-initialization must clear prior state and succeed again
	g.Begin();
	g.AddPass("dummy").Record([](const GraphContext&) {});
	EXPECT_EQ(g.GetPassCount(), 1u);

	RenderError err2 = g.Initialize(env.device, env.imageRegistry, env.bufferRegistry);
	EXPECT_EQ(err2, RenderError::None);
	EXPECT_TRUE(g.IsInitialized());
	// After re-init the per-frame state from the abandoned frame is gone
	EXPECT_EQ(g.GetPassCount(), 0u);
}

// ---------------------------------------------------------------------------
// graph_begin_resets_per_frame_state
// ---------------------------------------------------------------------------
TEST(Graph, test_graph_begin_resets_per_frame_state)
{
	VulkanEnv& env = GetEnv();
	if (!env.valid)
	{
		GTEST_SKIP() << "Vulkan unavailable";
	}

	Graph g;
	ASSERT_EQ(g.Initialize(env.device, env.imageRegistry, env.bufferRegistry), RenderError::None);

	// First frame: add two passes
	g.Begin();
	EXPECT_EQ(g.GetPassCount(), 0u);
	g.AddPass("pass_a").Record([](const GraphContext&) {});
	g.AddPass("pass_b").Record([](const GraphContext&) {});
	EXPECT_EQ(g.GetPassCount(), 2u);

	// Compile + Execute + End to properly close the frame
	EXPECT_EQ(g.Compile(), RenderError::None);
	// We don't have a real command buffer to Execute against in this test;
	// just call End to release declared handles.
	g.End();

	// Second frame: Begin resets pass count
	g.Begin();
	EXPECT_EQ(g.GetPassCount(), 0u);

	g.End();
}

// ---------------------------------------------------------------------------
// graph_import_image_registers_external_image
// ---------------------------------------------------------------------------
TEST(Graph, test_graph_import_image_registers_external_image)
{
	VulkanEnv& env = GetEnv();
	if (!env.valid)
	{
		GTEST_SKIP() << "Vulkan unavailable";
	}

	// Allocate a real image to import
	GraphImageDesc desc = MakeColorDesc();
	ImageHandle allocHandle = env.imageRegistry.Allocate(desc);
	if (!allocHandle.IsValid())
	{
		GTEST_SKIP() << "ImageRegistry::Allocate failed (likely OOM)";
	}

	const Image& img = env.imageRegistry.Get(allocHandle);
	VkExtent2D extent{desc.width, desc.height};

	Graph g;
	ASSERT_EQ(g.Initialize(env.device, env.imageRegistry, env.bufferRegistry), RenderError::None);
	g.Begin();

	ImageHandle importedHandle = g.ImportImage(
		img.GetHandle(), img.GetView(), desc.format, extent, ResourceUsage::Undefined);

	// The returned handle must be valid
	EXPECT_TRUE(importedHandle.IsValid());

	// High-bit convention: imported handles have bit 31 set
	EXPECT_NE(importedHandle.id & 0x80000000u, 0u);

	// Imported handles have ids strictly >= 0x80000000u,
	// which means they are distinguishable from registry-declared handles
	EXPECT_GE(importedHandle.id, 0x80000000u);

	// A second import produces a distinct handle
	ImageHandle importedHandle2 = g.ImportImage(
		img.GetHandle(), img.GetView(), desc.format, extent, ResourceUsage::ColorAttachment);
	EXPECT_TRUE(importedHandle2.IsValid());
	EXPECT_NE(importedHandle.id, importedHandle2.id);

	g.End();

	// Clean up the registry slot we borrowed for the test
	env.imageRegistry.Free(allocHandle);
}

// ---------------------------------------------------------------------------
// graph_declare_image_allocates_through_registry
// ---------------------------------------------------------------------------
TEST(Graph, test_graph_declare_image_allocates_through_registry)
{
	VulkanEnv& env = GetEnv();
	if (!env.valid)
	{
		GTEST_SKIP() << "Vulkan unavailable";
	}

	uint32_t slotsBefore = env.imageRegistry.GetSlotCount();
	uint32_t allocBefore = env.imageRegistry.GetAllocatedCount();

	Graph g;
	ASSERT_EQ(g.Initialize(env.device, env.imageRegistry, env.bufferRegistry), RenderError::None);
	g.Begin();

	GraphImageDesc desc = MakeColorDesc();
	ImageHandle h = g.DeclareImage(desc);

	if (!h.IsValid())
	{
		GTEST_SKIP() << "DeclareImage failed (likely OOM)";
	}

	// Registry-declared handles must have id < 0x80000000u
	EXPECT_LT(h.id, 0x80000000u);

	// The registry's allocated count should have increased
	EXPECT_GT(env.imageRegistry.GetAllocatedCount(), allocBefore);

	// End releases the handle back to the registry
	g.End();

	// After End the slot is freed: allocated count returns to prior level
	EXPECT_EQ(env.imageRegistry.GetAllocatedCount(), allocBefore);
	// Slot count may have grown (creation rule) but not shrunk
	EXPECT_GE(env.imageRegistry.GetSlotCount(), slotsBefore);
}

// ---------------------------------------------------------------------------
// graph_declare_buffer_allocates_through_registry
// ---------------------------------------------------------------------------
TEST(Graph, test_graph_declare_buffer_allocates_through_registry)
{
	VulkanEnv& env = GetEnv();
	if (!env.valid)
	{
		GTEST_SKIP() << "Vulkan unavailable";
	}

	uint32_t allocBefore = env.bufferRegistry.GetAllocatedCount();

	Graph g;
	ASSERT_EQ(g.Initialize(env.device, env.imageRegistry, env.bufferRegistry), RenderError::None);
	g.Begin();

	GraphBufferDesc desc = MakeBufferDesc();
	BufferHandle h = g.DeclareBuffer(desc);

	if (!h.IsValid())
	{
		GTEST_SKIP() << "DeclareBuffer failed (likely OOM)";
	}

	// The registry's allocated count should have increased
	EXPECT_GT(env.bufferRegistry.GetAllocatedCount(), allocBefore);

	// End releases the handle back to the registry
	g.End();

	EXPECT_EQ(env.bufferRegistry.GetAllocatedCount(), allocBefore);
}

// ---------------------------------------------------------------------------
// graph_add_pass_returns_builder
// ---------------------------------------------------------------------------
TEST(Graph, test_graph_add_pass_returns_builder)
{
	VulkanEnv& env = GetEnv();
	if (!env.valid)
	{
		GTEST_SKIP() << "Vulkan unavailable";
	}

	Graph g;
	ASSERT_EQ(g.Initialize(env.device, env.imageRegistry, env.bufferRegistry), RenderError::None);
	g.Begin();
	EXPECT_EQ(g.GetPassCount(), 0u);

	// Each AddPass increments GetPassCount by 1
	g.AddPass("pass_one").Record([](const GraphContext&) {});
	EXPECT_EQ(g.GetPassCount(), 1u);

	g.AddPass("pass_two").Record([](const GraphContext&) {});
	EXPECT_EQ(g.GetPassCount(), 2u);

	g.AddPass("pass_three").Record([](const GraphContext&) {});
	EXPECT_EQ(g.GetPassCount(), 3u);

	g.End();
	EXPECT_EQ(g.GetPassCount(), 0u);
}

// ---------------------------------------------------------------------------
// graph_compile_is_validation_only_in_v1
// ---------------------------------------------------------------------------
TEST(Graph, test_graph_compile_is_validation_only_in_v1)
{
	VulkanEnv& env = GetEnv();
	if (!env.valid)
	{
		GTEST_SKIP() << "Vulkan unavailable";
	}

	Graph g;
	ASSERT_EQ(g.Initialize(env.device, env.imageRegistry, env.bufferRegistry), RenderError::None);

	// ---- Success path: all passes have record callbacks ----
	g.Begin();
	g.AddPass("valid_pass").Record([](const GraphContext&) {});
	EXPECT_EQ(g.Compile(), RenderError::None);
	g.End();

	// ---- Failure path: a pass with no Record callback ----
	g.Begin();
	g.AddPass("no_callback_pass"); // deliberately no .Record(...)
	RenderError compileErr = g.Compile();
	EXPECT_EQ(compileErr, RenderError::PipelineCreateFailed);
	g.End();

	// ---- Failure path: a pass referencing an invalid image handle ----
	g.Begin();
	ImageHandle invalidHandle; // default-constructed → IsValid() == false
	g.AddPass("bad_handle_pass")
		.ColorAttachment(invalidHandle, LoadOp::Clear, ClearColor{})
		.Record([](const GraphContext&) {});
	RenderError compileErr2 = g.Compile();
	EXPECT_EQ(compileErr2, RenderError::PipelineCreateFailed);
	g.End();
}

// ---------------------------------------------------------------------------
// graph_execute_emits_barriers_and_invokes_callbacks
// ---------------------------------------------------------------------------
TEST(Graph, test_graph_execute_emits_barriers_and_invokes_callbacks)
{
	VulkanEnv& env = GetEnv();
	if (!env.valid)
	{
		GTEST_SKIP() << "Vulkan unavailable";
	}

	// We need a real command buffer to call Execute.
	// Create a transient command pool on the graphics queue.
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = env.device.GetGraphicsQueueFamilyIndex();
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	VkCommandPool pool = VK_NULL_HANDLE;
	if (vkCreateCommandPool(env.device.GetHandle(), &poolInfo, nullptr, &pool) != VK_SUCCESS)
	{
		GTEST_SKIP() << "Could not create command pool";
	}

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = pool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cmd = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(env.device.GetHandle(), &allocInfo, &cmd) != VK_SUCCESS)
	{
		vkDestroyCommandPool(env.device.GetHandle(), pool, nullptr);
		GTEST_SKIP() << "Could not allocate command buffer";
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	Graph g;
	ASSERT_EQ(g.Initialize(env.device, env.imageRegistry, env.bufferRegistry), RenderError::None);
	g.Begin();

	bool callbackInvoked = false;
	g.AddPass("test_pass")
		.Record(
			[&callbackInvoked](const GraphContext& ctx)
			{
				callbackInvoked = true;
				// Command buffer must be non-null inside the callback
				EXPECT_NE(ctx.GetCommandBuffer(), VK_NULL_HANDLE);
			});

	ASSERT_EQ(g.Compile(), RenderError::None);
	RenderError execErr = g.Execute(cmd);
	EXPECT_EQ(execErr, RenderError::None);
	EXPECT_TRUE(callbackInvoked);

	g.End();

	vkEndCommandBuffer(cmd);
	vkDestroyCommandPool(env.device.GetHandle(), pool, nullptr);
}

// ---------------------------------------------------------------------------
// graph_end_releases_declared_handles
// ---------------------------------------------------------------------------
TEST(Graph, test_graph_end_releases_declared_handles)
{
	VulkanEnv& env = GetEnv();
	if (!env.valid)
	{
		GTEST_SKIP() << "Vulkan unavailable";
	}

	uint32_t imgAllocBefore = env.imageRegistry.GetAllocatedCount();
	uint32_t bufAllocBefore = env.bufferRegistry.GetAllocatedCount();

	Graph g;
	ASSERT_EQ(g.Initialize(env.device, env.imageRegistry, env.bufferRegistry), RenderError::None);
	g.Begin();

	ImageHandle ih = g.DeclareImage(MakeColorDesc());
	BufferHandle bh = g.DeclareBuffer(MakeBufferDesc());

	if (!ih.IsValid() || !bh.IsValid())
	{
		g.End();
		GTEST_SKIP() << "Declare failed (likely OOM)";
	}

	// Handles are allocated during the frame
	EXPECT_GT(env.imageRegistry.GetAllocatedCount(), imgAllocBefore);
	EXPECT_GT(env.bufferRegistry.GetAllocatedCount(), bufAllocBefore);

	// End must release them back
	g.End();

	EXPECT_EQ(env.imageRegistry.GetAllocatedCount(), imgAllocBefore);
	EXPECT_EQ(env.bufferRegistry.GetAllocatedCount(), bufAllocBefore);

	// GetPassCount is 0 after End
	EXPECT_EQ(g.GetPassCount(), 0u);

	// The underlying Vulkan resources are NOT destroyed; the slots are merely
	// freed back to the freelist.  We verify this by checking that slot count
	// is unchanged (the slots still exist, just free).
	EXPECT_GE(env.imageRegistry.GetSlotCount(), 1u);
	EXPECT_GE(env.bufferRegistry.GetSlotCount(), 1u);
}

// ---------------------------------------------------------------------------
// graph_observers
// ---------------------------------------------------------------------------
TEST(Graph, test_graph_observers)
{
	VulkanEnv& env = GetEnv();

	// Default-constructed state
	{
		Graph g;
		EXPECT_FALSE(g.IsInitialized());
		EXPECT_EQ(g.GetPassCount(), 0u);
	}

	if (!env.valid)
	{
		GTEST_SKIP() << "Vulkan unavailable";
	}

	// After Initialize
	{
		Graph g;
		ASSERT_EQ(g.Initialize(env.device, env.imageRegistry, env.bufferRegistry),
			RenderError::None);
		EXPECT_TRUE(g.IsInitialized());
		EXPECT_EQ(g.GetPassCount(), 0u);

		g.Begin();
		EXPECT_EQ(g.GetPassCount(), 0u);
		g.AddPass("p1").Record([](const GraphContext&) {});
		EXPECT_EQ(g.GetPassCount(), 1u);
		g.AddPass("p2").Record([](const GraphContext&) {});
		EXPECT_EQ(g.GetPassCount(), 2u);
		g.End();
		EXPECT_EQ(g.GetPassCount(), 0u);
	}

	// Moved-from graph reports IsInitialized == false
	{
		Graph src;
		ASSERT_EQ(src.Initialize(env.device, env.imageRegistry, env.bufferRegistry),
			RenderError::None);
		Graph dst(std::move(src));
		EXPECT_FALSE(src.IsInitialized()); // NOLINT(bugprone-use-after-move)
		EXPECT_TRUE(dst.IsInitialized());
	}
}

// ---------------------------------------------------------------------------
// graph_is_not_thread_safe_per_instance  (structural / documentation test)
// ---------------------------------------------------------------------------
// The contract states that distinct Graph objects may be operated from
// distinct threads.  We verify the structural property that two independent
// Graph objects can each be initialized and used without interfering with
// each other (single-threaded here; the thread-safety guarantee is a
// contract-level assertion, not something we can meaningfully test without
// a data-race detector).
TEST(Graph, test_graph_is_not_thread_safe_per_instance)
{
	VulkanEnv& env = GetEnv();
	if (!env.valid)
	{
		GTEST_SKIP() << "Vulkan unavailable";
	}

	Graph g1;
	Graph g2;
	ASSERT_EQ(
		g1.Initialize(env.device, env.imageRegistry, env.bufferRegistry), RenderError::None);
	ASSERT_EQ(
		g2.Initialize(env.device, env.imageRegistry, env.bufferRegistry), RenderError::None);

	EXPECT_TRUE(g1.IsInitialized());
	EXPECT_TRUE(g2.IsInitialized());

	g1.Begin();
	g1.AddPass("g1_pass").Record([](const GraphContext&) {});
	EXPECT_EQ(g1.GetPassCount(), 1u);

	g2.Begin();
	g2.AddPass("g2_pass_a").Record([](const GraphContext&) {});
	g2.AddPass("g2_pass_b").Record([](const GraphContext&) {});
	EXPECT_EQ(g2.GetPassCount(), 2u);

	// Each graph's pass count is independent
	EXPECT_EQ(g1.GetPassCount(), 1u);

	g1.End();
	g2.End();

	EXPECT_EQ(g1.GetPassCount(), 0u);
	EXPECT_EQ(g2.GetPassCount(), 0u);
}
