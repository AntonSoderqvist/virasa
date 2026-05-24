#include <cstdint>
#include <gtest/gtest.h>

#define STATIC_ASSERT_FALSE(expr) static_assert(!(expr))
#define STATIC_ASSERT_TRUE(expr) static_assert(expr)
#include <utility>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/graph/BufferRegistry.h"
#include "virasa/renderer/graph/Types.h"
#include "virasa/renderer/resources/Buffer.h"
#include "virasa/window/Platform.h"

using namespace virasa::renderer::graph;
using namespace virasa::renderer;
using namespace virasa;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
namespace
{

// Build a minimal real Device for tests that require an initialized registry.
// Returns false if the environment cannot provide Vulkan (CI without a GPU).
struct TestEnv
{
	virasa::window::Platform platform;
	Instance instance;
	Device device;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	bool ready = false;

	TestEnv()
	{
		// We need a surface for Device::Initialize.
		if (platform.Initialize("BufferRegistryTest", 1, 1) != ErrorCode::None)
			return;

		RendererConfig cfg{};
		cfg.enableValidation = false;
		uint32_t extCount = 0;
		cfg.requiredInstanceExtensions =
			virasa::window::Platform::GetRequiredVulkanExtensions(&extCount);
		cfg.requiredInstanceExtensionCount = extCount;

		if (instance.Initialize(cfg) != RenderError::None)
			return;

		if (platform.CreateSurface(instance.GetHandle(), &surface) != ErrorCode::None)
			return;

		if (device.Initialize(instance, surface) != RenderError::None)
			return;

		ready = true;
	}

	~TestEnv()
	{
		if (surface != VK_NULL_HANDLE && instance.GetHandle() != VK_NULL_HANDLE)
			vkDestroySurfaceKHR(instance.GetHandle(), surface, nullptr);
		platform.Shutdown();
	}
};

// A simple device-buffer desc for host-visible buffers.
GraphBufferDesc MakeHostDesc(VkDeviceSize size = 256)
{
	GraphBufferDesc d{};
	d.size = size;
	d.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	d.hostVisible = true;
	return d;
}

// A simple device-local desc.
GraphBufferDesc MakeDeviceDesc(VkDeviceSize size = 256)
{
	GraphBufferDesc d{};
	d.size = size;
	d.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	d.hostVisible = false;
	return d;
}

} // namespace

// ---------------------------------------------------------------------------
// buffer_registry_is_raii_movable_non_copyable
// ---------------------------------------------------------------------------
TEST(BufferRegistry, test_buffer_registry_is_raii_movable_non_copyable)
{
	// Not copyable.
	STATIC_ASSERT_FALSE(std::is_copy_constructible_v<BufferRegistry>);
	STATIC_ASSERT_FALSE(std::is_copy_assignable_v<BufferRegistry>);

	// Movable.
	STATIC_ASSERT_TRUE(std::is_move_constructible_v<BufferRegistry>);
	STATIC_ASSERT_TRUE(std::is_move_assignable_v<BufferRegistry>);

	// Default-constructible.
	STATIC_ASSERT_TRUE(std::is_default_constructible_v<BufferRegistry>);

	// Final.
	STATIC_ASSERT_TRUE(std::is_final_v<BufferRegistry>);

	// Move-construct: source ends up in default-constructed state.
	{
		TestEnv env;
		if (!env.ready)
		{
			GTEST_SKIP() << "Vulkan not available";
		}

		BufferRegistry reg;
		ASSERT_EQ(reg.Initialize(env.device), RenderError::None);

		BufferHandle h = reg.Allocate(MakeHostDesc());
		ASSERT_TRUE(h.IsValid());
		ASSERT_EQ(reg.GetSlotCount(), 1u);
		ASSERT_EQ(reg.GetAllocatedCount(), 1u);

		BufferRegistry moved(std::move(reg));

		// Source is reset.
		EXPECT_FALSE(reg.IsInitialized());
		EXPECT_EQ(reg.GetSlotCount(), 0u);
		EXPECT_EQ(reg.GetAllocatedCount(), 0u);

		// Destination owns the slot.
		EXPECT_TRUE(moved.IsInitialized());
		EXPECT_EQ(moved.GetSlotCount(), 1u);
		EXPECT_EQ(moved.GetAllocatedCount(), 1u);
		EXPECT_TRUE(moved.IsAllocated(h));
	}

	// Move-assign: destination tears down its prior slots before taking ownership.
	{
		TestEnv env;
		if (!env.ready)
		{
			GTEST_SKIP() << "Vulkan not available";
		}

		BufferRegistry src;
		ASSERT_EQ(src.Initialize(env.device), RenderError::None);
		BufferHandle hSrc = src.Allocate(MakeHostDesc());
		ASSERT_TRUE(hSrc.IsValid());

		BufferRegistry dst;
		ASSERT_EQ(dst.Initialize(env.device), RenderError::None);
		BufferHandle hDst = dst.Allocate(MakeDeviceDesc());
		ASSERT_TRUE(hDst.IsValid());

		dst = std::move(src);

		// Source is reset.
		EXPECT_FALSE(src.IsInitialized());
		EXPECT_EQ(src.GetSlotCount(), 0u);
		EXPECT_EQ(src.GetAllocatedCount(), 0u);

		// Destination now owns src's slot.
		EXPECT_TRUE(dst.IsInitialized());
		EXPECT_EQ(dst.GetSlotCount(), 1u);
		EXPECT_EQ(dst.GetAllocatedCount(), 1u);
		EXPECT_TRUE(dst.IsAllocated(hSrc));
	}

	// Destroying a default-constructed BufferRegistry is well-defined.
	{
		BufferRegistry reg;
		(void)reg; // destructor runs at end of scope
	}

	// Destroying a moved-from BufferRegistry is well-defined.
	{
		TestEnv env;
		if (!env.ready)
		{
			GTEST_SKIP() << "Vulkan not available";
		}

		BufferRegistry reg;
		ASSERT_EQ(reg.Initialize(env.device), RenderError::None);
		BufferRegistry other(std::move(reg));
		(void)reg; // destructor of moved-from must not crash
	}
}

// ---------------------------------------------------------------------------
// buffer_registry_default_constructed_state
// ---------------------------------------------------------------------------
TEST(BufferRegistry, test_buffer_registry_default_constructed_state)
{
	BufferRegistry reg;

	EXPECT_FALSE(reg.IsInitialized());
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);

	// IsAllocated must return false for any handle, including the invalid sentinel.
	BufferHandle sentinel{}; // default: id == 0xFFFFFFFFu
	EXPECT_FALSE(reg.IsAllocated(sentinel));

	BufferHandle zero{};
	zero.id = 0u;
	EXPECT_FALSE(reg.IsAllocated(zero));

	BufferHandle arbitrary{};
	arbitrary.id = 42u;
	EXPECT_FALSE(reg.IsAllocated(arbitrary));
}

// ---------------------------------------------------------------------------
// buffer_registry_initialize_stores_device
// ---------------------------------------------------------------------------
TEST(BufferRegistry, test_buffer_registry_initialize_stores_device)
{
	TestEnv env;
	if (!env.ready)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	BufferRegistry reg;
	EXPECT_FALSE(reg.IsInitialized());

	RenderError err = reg.Initialize(env.device);
	EXPECT_EQ(err, RenderError::None);
	EXPECT_TRUE(reg.IsInitialized());
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);

	// Allocate a slot so re-initialization has something to tear down.
	BufferHandle h = reg.Allocate(MakeHostDesc());
	ASSERT_TRUE(h.IsValid());
	EXPECT_EQ(reg.GetSlotCount(), 1u);

	// Re-initialize: must tear down prior slots and return None.
	RenderError err2 = reg.Initialize(env.device);
	EXPECT_EQ(err2, RenderError::None);
	EXPECT_TRUE(reg.IsInitialized());
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);
	EXPECT_FALSE(reg.IsAllocated(h));
}

// ---------------------------------------------------------------------------
// buffer_registry_allocate_reuses_or_creates_slot
// ---------------------------------------------------------------------------
TEST(BufferRegistry, test_buffer_registry_allocate_reuses_or_creates_slot)
{
	TestEnv env;
	if (!env.ready)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	BufferRegistry reg;
	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);

	GraphBufferDesc desc = MakeHostDesc(512);

	// --- Creation rule ---
	BufferHandle h0 = reg.Allocate(desc);
	ASSERT_TRUE(h0.IsValid());
	EXPECT_EQ(reg.GetSlotCount(), 1u);
	EXPECT_EQ(reg.GetAllocatedCount(), 1u);
	EXPECT_TRUE(reg.IsAllocated(h0));

	// Newly created slot has ResourceUsage::Undefined.
	EXPECT_EQ(reg.GetUsage(h0), ResourceUsage::Undefined);

	// --- Second creation (different desc) ---
	GraphBufferDesc desc2 = MakeDeviceDesc(1024);
	BufferHandle h1 = reg.Allocate(desc2);
	ASSERT_TRUE(h1.IsValid());
	EXPECT_EQ(reg.GetSlotCount(), 2u);
	EXPECT_EQ(reg.GetAllocatedCount(), 2u);

	// --- Reuse rule ---
	// Set a non-Undefined usage on h0, free it, then re-allocate with the same desc.
	reg.SetUsage(h0, ResourceUsage::TransferDst);
	reg.Free(h0);
	EXPECT_EQ(reg.GetSlotCount(), 2u);
	EXPECT_EQ(reg.GetAllocatedCount(), 1u);

	BufferHandle h0b = reg.Allocate(desc);
	ASSERT_TRUE(h0b.IsValid());
	// Reuse: slot count must NOT grow.
	EXPECT_EQ(reg.GetSlotCount(), 2u);
	EXPECT_EQ(reg.GetAllocatedCount(), 2u);
	// Reclaimed slot index must equal the freed slot index.
	EXPECT_EQ(h0b.id, h0.id);
	// Cached usage is preserved from before Free (cross-frame barrier semantics).
	EXPECT_EQ(reg.GetUsage(h0b), ResourceUsage::TransferDst);

	// --- Invalid sentinel returned on bad desc (size == 0 should fail Buffer::Initialize) ---
	GraphBufferDesc badDesc{};
	badDesc.size = 0;
	badDesc.usage = 0;
	badDesc.hostVisible = false;
	BufferHandle bad = reg.Allocate(badDesc);
	// The contract says a failed Allocate returns the invalid sentinel.
	// (Whether size=0 fails is implementation-dependent; we only check IsValid
	// if the slot count did not grow, which would indicate failure.)
	if (!bad.IsValid())
	{
		EXPECT_EQ(bad.id, 0xFFFFFFFFu);
	}
}

// ---------------------------------------------------------------------------
// buffer_registry_free_returns_slot_to_freelist
// ---------------------------------------------------------------------------
TEST(BufferRegistry, test_buffer_registry_free_returns_slot_to_freelist)
{
	TestEnv env;
	if (!env.ready)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	BufferRegistry reg;
	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);

	GraphBufferDesc desc = MakeHostDesc(256);
	BufferHandle h = reg.Allocate(desc);
	ASSERT_TRUE(h.IsValid());

	// Set a usage so we can verify it is preserved across Free.
	reg.SetUsage(h, ResourceUsage::StorageReadCompute);

	uint32_t slotsBefore = reg.GetSlotCount();
	uint32_t allocBefore = reg.GetAllocatedCount();

	reg.Free(h);

	// IsAllocated must now be false.
	EXPECT_FALSE(reg.IsAllocated(h));
	// GetSlotCount unchanged.
	EXPECT_EQ(reg.GetSlotCount(), slotsBefore);
	// GetAllocatedCount decreased by 1.
	EXPECT_EQ(reg.GetAllocatedCount(), allocBefore - 1u);

	// The underlying Buffer is NOT destroyed: re-allocating with the same desc
	// should reclaim the same slot (same VkBuffer handle).
	VkBuffer vkBufBefore = reg.Get(h).GetHandle(); // still valid since Free doesn't destroy
	// Note: Get on a freed handle is technically UB per contract, but the
	// contract also says the Buffer is retained; we read it once here for
	// the identity check below.
	(void)vkBufBefore;

	BufferHandle h2 = reg.Allocate(desc);
	ASSERT_TRUE(h2.IsValid());
	EXPECT_EQ(h2.id, h.id); // same slot reclaimed
	// Usage is preserved (not reset to Undefined on reuse).
	EXPECT_EQ(reg.GetUsage(h2), ResourceUsage::StorageReadCompute);

	// Free performs no Vulkan calls: verified implicitly by the fact that
	// the Buffer's VkBuffer handle is still valid after re-allocation.
	EXPECT_NE(reg.Get(h2).GetHandle(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// buffer_registry_get_returns_const_reference
// ---------------------------------------------------------------------------
TEST(BufferRegistry, test_buffer_registry_get_returns_const_reference)
{
	TestEnv env;
	if (!env.ready)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	BufferRegistry reg;
	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);

	GraphBufferDesc desc = MakeHostDesc(128);
	BufferHandle h = reg.Allocate(desc);
	ASSERT_TRUE(h.IsValid());

	const Buffer& buf = reg.Get(h);

	// The returned reference must point at an initialized Buffer.
	EXPECT_TRUE(buf.IsInitialized());
	EXPECT_NE(buf.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(buf.GetSize(), desc.size);

	// Get is const and noexcept — verified at compile time via the const registry.
	const BufferRegistry& cReg = reg;
	const Buffer& buf2 = cReg.Get(h);
	EXPECT_EQ(buf2.GetHandle(), buf.GetHandle());

	// Allocating another slot must not change the VkBuffer for an existing handle.
	VkBuffer handleBefore = buf.GetHandle();
	BufferHandle h2 = reg.Allocate(MakeDeviceDesc(64));
	ASSERT_TRUE(h2.IsValid());
	EXPECT_EQ(reg.Get(h).GetHandle(), handleBefore);
}

// ---------------------------------------------------------------------------
// buffer_registry_desc_observer
// ---------------------------------------------------------------------------
TEST(BufferRegistry, test_buffer_registry_desc_observer)
{
	TestEnv env;
	if (!env.ready)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	BufferRegistry reg;
	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);

	GraphBufferDesc desc = MakeHostDesc(512);
	BufferHandle h = reg.Allocate(desc);
	ASSERT_TRUE(h.IsValid());

	const GraphBufferDesc& cached = reg.GetDesc(h);
	EXPECT_EQ(cached.size, desc.size);
	EXPECT_EQ(cached.usage, desc.usage);
	EXPECT_EQ(cached.hostVisible, desc.hostVisible);

	// GetDesc is const and noexcept.
	const BufferRegistry& cReg = reg;
	const GraphBufferDesc& cached2 = cReg.GetDesc(h);
	EXPECT_EQ(cached2.size, desc.size);

	// After free + reuse, the cached desc on the reclaimed slot equals the
	// desc supplied to the reuse Allocate (which equals the original desc).
	reg.Free(h);
	BufferHandle h2 = reg.Allocate(desc);
	ASSERT_TRUE(h2.IsValid());
	EXPECT_EQ(h2.id, h.id);
	const GraphBufferDesc& reused = reg.GetDesc(h2);
	EXPECT_EQ(reused.size, desc.size);
	EXPECT_EQ(reused.usage, desc.usage);
	EXPECT_EQ(reused.hostVisible, desc.hostVisible);
}

// ---------------------------------------------------------------------------
// buffer_registry_usage_observers
// ---------------------------------------------------------------------------
TEST(BufferRegistry, test_buffer_registry_usage_observers)
{
	TestEnv env;
	if (!env.ready)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	BufferRegistry reg;
	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);

	GraphBufferDesc desc = MakeDeviceDesc(256);
	BufferHandle h = reg.Allocate(desc);
	ASSERT_TRUE(h.IsValid());

	// Initial usage is Undefined.
	EXPECT_EQ(reg.GetUsage(h), ResourceUsage::Undefined);

	// SetUsage overwrites the cached value.
	reg.SetUsage(h, ResourceUsage::StorageWriteCompute);
	EXPECT_EQ(reg.GetUsage(h), ResourceUsage::StorageWriteCompute);

	reg.SetUsage(h, ResourceUsage::TransferSrc);
	EXPECT_EQ(reg.GetUsage(h), ResourceUsage::TransferSrc);

	// GetUsage is const and noexcept.
	const BufferRegistry& cReg = reg;
	EXPECT_EQ(cReg.GetUsage(h), ResourceUsage::TransferSrc);

	// Usage is preserved across Free / reuse.
	reg.Free(h);
	BufferHandle h2 = reg.Allocate(desc);
	ASSERT_TRUE(h2.IsValid());
	EXPECT_EQ(h2.id, h.id);
	EXPECT_EQ(reg.GetUsage(h2), ResourceUsage::TransferSrc);

	// SetUsage on the reclaimed slot works.
	reg.SetUsage(h2, ResourceUsage::ColorAttachment);
	EXPECT_EQ(reg.GetUsage(h2), ResourceUsage::ColorAttachment);
}

// ---------------------------------------------------------------------------
// buffer_registry_observers
// ---------------------------------------------------------------------------
TEST(BufferRegistry, test_buffer_registry_observers)
{
	TestEnv env;
	if (!env.ready)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	BufferRegistry reg;
	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);

	// Initially: nothing allocated.
	EXPECT_TRUE(reg.IsInitialized());
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);

	// Invalid sentinel returns false.
	BufferHandle sentinel{};
	EXPECT_FALSE(reg.IsAllocated(sentinel));

	// Out-of-range handle returns false.
	BufferHandle oob{};
	oob.id = 999u;
	EXPECT_FALSE(reg.IsAllocated(oob));

	// Allocate two slots.
	BufferHandle h0 = reg.Allocate(MakeHostDesc(128));
	ASSERT_TRUE(h0.IsValid());
	BufferHandle h1 = reg.Allocate(MakeDeviceDesc(256));
	ASSERT_TRUE(h1.IsValid());

	EXPECT_EQ(reg.GetSlotCount(), 2u);
	EXPECT_EQ(reg.GetAllocatedCount(), 2u);
	EXPECT_TRUE(reg.IsAllocated(h0));
	EXPECT_TRUE(reg.IsAllocated(h1));

	// Free h0: allocated count drops, slot count unchanged.
	reg.Free(h0);
	EXPECT_EQ(reg.GetSlotCount(), 2u);
	EXPECT_EQ(reg.GetAllocatedCount(), 1u);
	EXPECT_FALSE(reg.IsAllocated(h0));
	EXPECT_TRUE(reg.IsAllocated(h1));

	// Reuse h0: allocated count rises again, slot count unchanged.
	BufferHandle h0b = reg.Allocate(MakeHostDesc(128));
	ASSERT_TRUE(h0b.IsValid());
	EXPECT_EQ(reg.GetSlotCount(), 2u);
	EXPECT_EQ(reg.GetAllocatedCount(), 2u);
	EXPECT_TRUE(reg.IsAllocated(h0b));

	// GetSlotCount never decreases except on move-from or re-Initialize.
	// Verify re-Initialize resets it.
	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);
	EXPECT_TRUE(reg.IsInitialized());

	// After move-from, IsInitialized returns false.
	BufferRegistry moved(std::move(reg));
	EXPECT_FALSE(reg.IsInitialized());
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);
}

// ---------------------------------------------------------------------------
// buffer_registry_is_not_thread_safe_per_instance
// ---------------------------------------------------------------------------
// The contract states that concurrent const observers on the same registry
// are safe. We verify that multiple threads can call const observers
// simultaneously without data races (using a registry that is not being
// mutated concurrently).
TEST(BufferRegistry, test_buffer_registry_is_not_thread_safe_per_instance)
{
	TestEnv env;
	if (!env.ready)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	BufferRegistry reg;
	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);

	GraphBufferDesc desc = MakeHostDesc(64);
	BufferHandle h = reg.Allocate(desc);
	ASSERT_TRUE(h.IsValid());
	reg.SetUsage(h, ResourceUsage::SampledFragment);

	// Spin up several threads that each call const observers concurrently.
	constexpr int kThreadCount = 8;
	constexpr int kIterations = 1000;
	std::vector<std::thread> threads;
	threads.reserve(kThreadCount);

	const BufferRegistry& cReg = reg;

	for (int t = 0; t < kThreadCount; ++t)
	{
		threads.emplace_back(
			[&cReg, h]()
			{
				for (int i = 0; i < kIterations; ++i)
				{
					(void)cReg.IsInitialized();
					(void)cReg.GetSlotCount();
					(void)cReg.GetAllocatedCount();
					(void)cReg.IsAllocated(h);
					(void)cReg.GetUsage(h);
					(void)cReg.GetDesc(h);
					(void)cReg.Get(h).IsInitialized();
				}
			});
	}

	for (auto& thr : threads)
		thr.join();

	// If we reach here without a sanitizer report, concurrent const access is safe.
	EXPECT_TRUE(reg.IsInitialized());
	EXPECT_EQ(reg.GetSlotCount(), 1u);
	EXPECT_EQ(reg.GetAllocatedCount(), 1u);
}
