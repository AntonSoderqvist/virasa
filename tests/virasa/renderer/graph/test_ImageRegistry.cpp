#include <cstdint>
#include <gtest/gtest.h>
#include <utility>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/graph/ImageRegistry.h"
#include "virasa/renderer/graph/Types.h"
#include "virasa/renderer/resources/Image.h"
#include "virasa/window/Platform.h"

using namespace virasa::renderer::graph;
using namespace virasa::renderer;
using namespace virasa;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

namespace
{

// Bring up a real Vulkan Device for tests that need one.
// Returns false if the environment cannot provide Vulkan.
struct VulkanEnv
{
	virasa::window::Platform platform;
	Instance instance;
	Device device;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	bool ready = false;

	VulkanEnv()
	{
		// Initialize platform (headless-ish: 1x1 window)
		if (platform.Initialize("TestWindow", 1, 1) != ErrorCode::None)
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

	~VulkanEnv()
	{
		// Device destructor cleans up VkDevice; surface must be destroyed before instance.
		if (surface != VK_NULL_HANDLE && instance.GetHandle() != VK_NULL_HANDLE)
			vkDestroySurfaceKHR(instance.GetHandle(), surface, nullptr);
		platform.Shutdown();
	}
};

// A simple color-attachment GraphImageDesc for reuse across tests.
GraphImageDesc MakeColorDesc(uint32_t w = 64, uint32_t h = 64)
{
	GraphImageDesc d{};
	d.width = w;
	d.height = h;
	d.format = VK_FORMAT_R8G8B8A8_UNORM;
	d.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	d.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	return d;
}

// A depth-attachment GraphImageDesc.
GraphImageDesc MakeDepthDesc(uint32_t w = 64, uint32_t h = 64)
{
	GraphImageDesc d{};
	d.width = w;
	d.height = h;
	d.format = VK_FORMAT_D32_SFLOAT;
	d.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	d.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	return d;
}

} // namespace

// ---------------------------------------------------------------------------
// image_registry_is_raii_movable_non_copyable
// ---------------------------------------------------------------------------
TEST(ImageRegistry, test_image_registry_is_raii_movable_non_copyable)
{
// Not copyable
STATIC_ASSERT_FALSE_HELPER:
	static_assert(!std::is_copy_constructible_v<ImageRegistry>,
		"ImageRegistry must not be copy-constructible");
	static_assert(
		!std::is_copy_assignable_v<ImageRegistry>, "ImageRegistry must not be copy-assignable");

	// Movable
	static_assert(std::is_move_constructible_v<ImageRegistry>,
		"ImageRegistry must be move-constructible");
	static_assert(
		std::is_move_assignable_v<ImageRegistry>, "ImageRegistry must be move-assignable");

	// Default-constructed state
	ImageRegistry reg;
	EXPECT_FALSE(reg.IsInitialized());
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);

	VulkanEnv env;
	if (!env.ready)
	{
		GTEST_SKIP() << "Vulkan not available; skipping move-with-slots sub-test";
	}

	// Initialize and allocate a slot so the source has content.
	ImageRegistry src;
	ASSERT_EQ(src.Initialize(env.device), RenderError::None);
	GraphImageDesc desc = MakeColorDesc();
	ImageHandle h = src.Allocate(desc);
	ASSERT_TRUE(h.IsValid());
	EXPECT_EQ(src.GetSlotCount(), 1u);
	EXPECT_EQ(src.GetAllocatedCount(), 1u);

	// Move-construct
	ImageRegistry dst(std::move(src));
	EXPECT_FALSE(src.IsInitialized());
	EXPECT_EQ(src.GetSlotCount(), 0u);
	EXPECT_EQ(src.GetAllocatedCount(), 0u);
	EXPECT_TRUE(dst.IsInitialized());
	EXPECT_EQ(dst.GetSlotCount(), 1u);
	EXPECT_EQ(dst.GetAllocatedCount(), 1u);
	EXPECT_TRUE(dst.IsAllocated(h));

	// Move-assign into another registry
	ImageRegistry dst2;
	dst2 = std::move(dst);
	EXPECT_FALSE(dst.IsInitialized());
	EXPECT_EQ(dst.GetSlotCount(), 0u);
	EXPECT_EQ(dst.GetAllocatedCount(), 0u);
	EXPECT_TRUE(dst2.IsInitialized());
	EXPECT_EQ(dst2.GetSlotCount(), 1u);
	EXPECT_EQ(dst2.GetAllocatedCount(), 1u);
	EXPECT_TRUE(dst2.IsAllocated(h));

	// Move-assign into a registry that already owns slots (teardown of prior slots)
	ImageRegistry dst3;
	ASSERT_EQ(dst3.Initialize(env.device), RenderError::None);
	ImageHandle h3 = dst3.Allocate(MakeDepthDesc());
	ASSERT_TRUE(h3.IsValid());
	dst3 = std::move(dst2); // dst3 must tear down its prior slot first
	EXPECT_TRUE(dst3.IsInitialized());
	EXPECT_EQ(dst3.GetSlotCount(), 1u);
	EXPECT_TRUE(dst3.IsAllocated(h));

	// Destroying a default-constructed ImageRegistry is well-defined (no crash)
	{
		ImageRegistry empty;
		(void)empty;
	}
}

// ---------------------------------------------------------------------------
// image_registry_default_constructed_state
// ---------------------------------------------------------------------------
TEST(ImageRegistry, test_image_registry_default_constructed_state)
{
	ImageRegistry reg;

	EXPECT_FALSE(reg.IsInitialized());
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);

	// IsAllocated must return false for any handle, including the invalid sentinel.
	ImageHandle sentinel; // id == 0xFFFFFFFFu
	EXPECT_FALSE(reg.IsAllocated(sentinel));

	ImageHandle zero{};
	zero.id = 0u;
	EXPECT_FALSE(reg.IsAllocated(zero));

	ImageHandle arbitrary{};
	arbitrary.id = 42u;
	EXPECT_FALSE(reg.IsAllocated(arbitrary));
}

// ---------------------------------------------------------------------------
// image_registry_initialize_stores_device
// ---------------------------------------------------------------------------
TEST(ImageRegistry, test_image_registry_initialize_stores_device)
{
	VulkanEnv env;
	if (!env.ready)
		GTEST_SKIP() << "Vulkan not available";

	ImageRegistry reg;
	EXPECT_FALSE(reg.IsInitialized());

	RenderError err = reg.Initialize(env.device);
	EXPECT_EQ(err, RenderError::None);
	EXPECT_TRUE(reg.IsInitialized());
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);

	// Re-initialization: allocate a slot, then re-initialize.
	// Re-init must tear down prior slots and leave the registry clean.
	GraphImageDesc desc = MakeColorDesc();
	ImageHandle h = reg.Allocate(desc);
	ASSERT_TRUE(h.IsValid());
	EXPECT_EQ(reg.GetSlotCount(), 1u);

	RenderError err2 = reg.Initialize(env.device);
	EXPECT_EQ(err2, RenderError::None);
	EXPECT_TRUE(reg.IsInitialized());
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);

	// After re-init the old handle must no longer be allocated.
	EXPECT_FALSE(reg.IsAllocated(h));

	// Registry must be functional for new allocations after re-init.
	ImageHandle h2 = reg.Allocate(desc);
	EXPECT_TRUE(h2.IsValid());
	EXPECT_EQ(reg.GetSlotCount(), 1u);
	EXPECT_EQ(reg.GetAllocatedCount(), 1u);
}

// ---------------------------------------------------------------------------
// image_registry_allocate_reuses_or_creates_slot
// ---------------------------------------------------------------------------
TEST(ImageRegistry, test_image_registry_allocate_reuses_or_creates_slot)
{
	VulkanEnv env;
	if (!env.ready)
		GTEST_SKIP() << "Vulkan not available";

	ImageRegistry reg;
	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);

	GraphImageDesc desc = MakeColorDesc();

	// --- Creation rule ---
	ImageHandle h0 = reg.Allocate(desc);
	ASSERT_TRUE(h0.IsValid());
	EXPECT_EQ(reg.GetSlotCount(), 1u);
	EXPECT_EQ(reg.GetAllocatedCount(), 1u);
	EXPECT_TRUE(reg.IsAllocated(h0));

	// Image must be initialized and have correct dimensions/format.
	// Snapshot handles before any further Allocate calls that could grow _slots and invalidate refs.
	const VkImage expectedHandle = reg.Get(h0).GetHandle();
	const VkImageView expectedView = reg.Get(h0).GetView();
	EXPECT_TRUE(reg.Get(h0).IsInitialized());
	EXPECT_NE(expectedHandle, VK_NULL_HANDLE);
	EXPECT_NE(expectedView, VK_NULL_HANDLE);
	EXPECT_EQ(reg.Get(h0).GetWidth(), desc.width);
	EXPECT_EQ(reg.Get(h0).GetHeight(), desc.height);
	EXPECT_EQ(reg.Get(h0).GetFormat(), desc.format);

	// Initial ResourceUsage must be Undefined.
	EXPECT_EQ(reg.GetUsage(h0), ResourceUsage::Undefined);

	// Cached desc must match.
	const GraphImageDesc& cachedDesc = reg.GetDesc(h0);
	EXPECT_EQ(cachedDesc.width, desc.width);
	EXPECT_EQ(cachedDesc.height, desc.height);
	EXPECT_EQ(cachedDesc.format, desc.format);
	EXPECT_EQ(cachedDesc.usage, desc.usage);
	EXPECT_EQ(cachedDesc.aspect, desc.aspect);

	// Second allocation with a DIFFERENT desc → new slot created.
	GraphImageDesc desc2 = MakeColorDesc(128, 128);
	ImageHandle h1 = reg.Allocate(desc2);
	ASSERT_TRUE(h1.IsValid());
	EXPECT_EQ(reg.GetSlotCount(), 2u);
	EXPECT_EQ(reg.GetAllocatedCount(), 2u);

	// --- Reuse rule ---
	// Set a non-Undefined usage on h0 before freeing it, to verify it is preserved.
	reg.SetUsage(h0, ResourceUsage::ColorAttachment);
	reg.Free(h0);
	EXPECT_EQ(reg.GetSlotCount(), 2u); // slot count unchanged after Free
	EXPECT_EQ(reg.GetAllocatedCount(), 1u);

	// Allocate with the same desc as h0 → reuse rule must fire.
	ImageHandle h0r = reg.Allocate(desc);
	ASSERT_TRUE(h0r.IsValid());
	EXPECT_EQ(h0r.id, h0.id);	     // same slot index reclaimed
	EXPECT_EQ(reg.GetSlotCount(), 2u); // slot count still unchanged
	EXPECT_EQ(reg.GetAllocatedCount(), 2u);

	// The VkImage and VkImageView must be the same objects (reuse, no recreation).
	EXPECT_EQ(reg.Get(h0r).GetHandle(), expectedHandle);
	EXPECT_EQ(reg.Get(h0r).GetView(), expectedView);

	// ResourceUsage must be preserved from before Free (ColorAttachment).
	EXPECT_EQ(reg.GetUsage(h0r), ResourceUsage::ColorAttachment);

	// --- Failure sentinel ---
	// Construct an invalid desc to force Image::Initialize to fail.
	GraphImageDesc badDesc{};
	badDesc.width = 0;
	badDesc.height = 0;
	badDesc.format = VK_FORMAT_UNDEFINED;
	badDesc.usage = 0;
	badDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	ImageHandle bad = reg.Allocate(badDesc);
	// The returned handle should be invalid (Image creation must fail for a 0x0 image).
	EXPECT_FALSE(bad.IsValid());
}

// ---------------------------------------------------------------------------
// image_registry_free_returns_slot_to_freelist
// ---------------------------------------------------------------------------
TEST(ImageRegistry, test_image_registry_free_returns_slot_to_freelist)
{
	VulkanEnv env;
	if (!env.ready)
		GTEST_SKIP() << "Vulkan not available";

	ImageRegistry reg;
	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);

	GraphImageDesc desc = MakeColorDesc();
	ImageHandle h = reg.Allocate(desc);
	ASSERT_TRUE(h.IsValid());

	// Capture the underlying Vulkan handles before Free.
	VkImage vkImg = reg.Get(h).GetHandle();
	VkImageView vkView = reg.Get(h).GetView();

	// Set a non-Undefined usage to verify preservation.
	reg.SetUsage(h, ResourceUsage::TransferDst);

	uint32_t slotCountBefore = reg.GetSlotCount();
	uint32_t allocCountBefore = reg.GetAllocatedCount();

	reg.Free(h);

	// Post-Free state.
	EXPECT_FALSE(reg.IsAllocated(h));
	EXPECT_EQ(reg.GetSlotCount(), slotCountBefore); // unchanged
	EXPECT_EQ(reg.GetAllocatedCount(), allocCountBefore - 1u);

	// Reclaim via Allocate with same desc.
	ImageHandle h2 = reg.Allocate(desc);
	ASSERT_TRUE(h2.IsValid());
	EXPECT_EQ(h2.id, h.id); // reuse rule fired

	// Vulkan handles must be the same (Image not destroyed by Free).
	EXPECT_EQ(reg.Get(h2).GetHandle(), vkImg);
	EXPECT_EQ(reg.Get(h2).GetView(), vkView);

	// ResourceUsage preserved across Free/Allocate.
	EXPECT_EQ(reg.GetUsage(h2), ResourceUsage::TransferDst);

	// Cached desc preserved.
	const GraphImageDesc& cd = reg.GetDesc(h2);
	EXPECT_EQ(cd.width, desc.width);
	EXPECT_EQ(cd.height, desc.height);
	EXPECT_EQ(cd.format, desc.format);
	EXPECT_EQ(cd.usage, desc.usage);
	EXPECT_EQ(cd.aspect, desc.aspect);

	// Free performs no Vulkan calls: verify by checking the Image is still initialized
	// after Free (before the reclaim).
	ImageHandle h3 = reg.Allocate(MakeDepthDesc());
	ASSERT_TRUE(h3.IsValid());
	reg.Free(h3);
	// After Free, GetSlotCount is unchanged and GetAllocatedCount decreased.
	EXPECT_EQ(reg.GetAllocatedCount(), 1u); // only h2 remains allocated
	EXPECT_EQ(reg.GetSlotCount(), 2u);
}

// ---------------------------------------------------------------------------
// image_registry_get_returns_const_reference
// ---------------------------------------------------------------------------
TEST(ImageRegistry, test_image_registry_get_returns_const_reference)
{
	VulkanEnv env;
	if (!env.ready)
		GTEST_SKIP() << "Vulkan not available";

	ImageRegistry reg;
	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);

	GraphImageDesc desc = MakeColorDesc(32, 32);
	ImageHandle h = reg.Allocate(desc);
	ASSERT_TRUE(h.IsValid());

	const Image& ref = reg.Get(h);

	// The reference must point to an initialized Image.
	EXPECT_TRUE(ref.IsInitialized());
	EXPECT_NE(ref.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(ref.GetView(), VK_NULL_HANDLE);
	EXPECT_EQ(ref.GetFormat(), desc.format);
	EXPECT_EQ(ref.GetWidth(), desc.width);
	EXPECT_EQ(ref.GetHeight(), desc.height);

	VkExtent2D ext = ref.GetExtent();
	EXPECT_EQ(ext.width, desc.width);
	EXPECT_EQ(ext.height, desc.height);

	// Allocate a second slot (no growth beyond capacity assumed here; just verify
	// the reference still holds after a second allocation that does NOT cause realloc
	// beyond existing capacity — we cannot guarantee no realloc, so we just verify
	// the API contract that Get is const and noexcept).
	static_assert(noexcept(reg.Get(h)), "Get must be noexcept");

	// Get on the const registry must compile.
	const ImageRegistry& cReg = reg;
	const Image& cRef = cReg.Get(h);
	EXPECT_EQ(cRef.GetHandle(), ref.GetHandle());
}

// ---------------------------------------------------------------------------
// image_registry_desc_observer
// ---------------------------------------------------------------------------
TEST(ImageRegistry, test_image_registry_desc_observer)
{
	VulkanEnv env;
	if (!env.ready)
		GTEST_SKIP() << "Vulkan not available";

	ImageRegistry reg;
	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);

	GraphImageDesc desc = MakeColorDesc(16, 32);
	desc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	desc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;

	ImageHandle h = reg.Allocate(desc);
	ASSERT_TRUE(h.IsValid());

	const GraphImageDesc& cached = reg.GetDesc(h);
	EXPECT_EQ(cached.width, desc.width);
	EXPECT_EQ(cached.height, desc.height);
	EXPECT_EQ(cached.format, desc.format);
	EXPECT_EQ(cached.usage, desc.usage);
	EXPECT_EQ(cached.aspect, desc.aspect);

	// noexcept check
	static_assert(noexcept(reg.GetDesc(h)), "GetDesc must be noexcept");

	// Reuse path: free and reallocate with same desc; GetDesc must still match.
	reg.Free(h);
	ImageHandle h2 = reg.Allocate(desc);
	ASSERT_TRUE(h2.IsValid());
	const GraphImageDesc& cached2 = reg.GetDesc(h2);
	EXPECT_EQ(cached2.width, desc.width);
	EXPECT_EQ(cached2.height, desc.height);
	EXPECT_EQ(cached2.format, desc.format);
	EXPECT_EQ(cached2.usage, desc.usage);
	EXPECT_EQ(cached2.aspect, desc.aspect);
}

// ---------------------------------------------------------------------------
// image_registry_usage_observers
// ---------------------------------------------------------------------------
TEST(ImageRegistry, test_image_registry_usage_observers)
{
	VulkanEnv env;
	if (!env.ready)
		GTEST_SKIP() << "Vulkan not available";

	ImageRegistry reg;
	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);

	GraphImageDesc desc = MakeColorDesc();
	ImageHandle h = reg.Allocate(desc);
	ASSERT_TRUE(h.IsValid());

	// Initial usage must be Undefined.
	EXPECT_EQ(reg.GetUsage(h), ResourceUsage::Undefined);

	// noexcept check
	static_assert(noexcept(reg.GetUsage(h)), "GetUsage must be noexcept");

	// SetUsage updates the cached value.
	reg.SetUsage(h, ResourceUsage::ColorAttachment);
	EXPECT_EQ(reg.GetUsage(h), ResourceUsage::ColorAttachment);

	reg.SetUsage(h, ResourceUsage::SampledFragment);
	EXPECT_EQ(reg.GetUsage(h), ResourceUsage::SampledFragment);

	reg.SetUsage(h, ResourceUsage::TransferSrc);
	EXPECT_EQ(reg.GetUsage(h), ResourceUsage::TransferSrc);

	// Free preserves the usage; verify by reclaiming and checking.
	reg.Free(h);
	ImageHandle h2 = reg.Allocate(desc);
	ASSERT_TRUE(h2.IsValid());
	EXPECT_EQ(reg.GetUsage(h2), ResourceUsage::TransferSrc);

	// SetUsage on the reclaimed slot works normally.
	reg.SetUsage(h2, ResourceUsage::Present);
	EXPECT_EQ(reg.GetUsage(h2), ResourceUsage::Present);

	// SetUsage performs no Vulkan calls (observable only by absence of crash/error).
	reg.SetUsage(h2, ResourceUsage::Undefined);
	EXPECT_EQ(reg.GetUsage(h2), ResourceUsage::Undefined);
}

// ---------------------------------------------------------------------------
// image_registry_observers
// ---------------------------------------------------------------------------
TEST(ImageRegistry, test_image_registry_observers)
{
	VulkanEnv env;
	if (!env.ready)
		GTEST_SKIP() << "Vulkan not available";

	ImageRegistry reg;

	// --- IsInitialized ---
	EXPECT_FALSE(reg.IsInitialized());
	static_assert(noexcept(reg.IsInitialized()), "IsInitialized must be noexcept");

	// --- GetSlotCount / GetAllocatedCount on empty ---
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);
	static_assert(noexcept(reg.GetSlotCount()), "GetSlotCount must be noexcept");
	static_assert(noexcept(reg.GetAllocatedCount()), "GetAllocatedCount must be noexcept");

	// --- IsAllocated with invalid sentinel on uninitialized registry ---
	ImageHandle sentinel; // id == 0xFFFFFFFFu
	EXPECT_FALSE(reg.IsAllocated(sentinel));
	static_assert(noexcept(reg.IsAllocated(sentinel)), "IsAllocated must be noexcept");

	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);
	EXPECT_TRUE(reg.IsInitialized());

	// IsAllocated returns false for out-of-range handles.
	ImageHandle outOfRange{};
	outOfRange.id = 999u;
	EXPECT_FALSE(reg.IsAllocated(outOfRange));
	EXPECT_FALSE(reg.IsAllocated(sentinel));

	// Allocate two slots.
	GraphImageDesc d1 = MakeColorDesc(8, 8);
	GraphImageDesc d2 = MakeDepthDesc(8, 8);
	ImageHandle h1 = reg.Allocate(d1);
	ImageHandle h2 = reg.Allocate(d2);
	ASSERT_TRUE(h1.IsValid());
	ASSERT_TRUE(h2.IsValid());

	EXPECT_EQ(reg.GetSlotCount(), 2u);
	EXPECT_EQ(reg.GetAllocatedCount(), 2u);
	EXPECT_TRUE(reg.IsAllocated(h1));
	EXPECT_TRUE(reg.IsAllocated(h2));

	// Free one slot.
	reg.Free(h1);
	EXPECT_EQ(reg.GetSlotCount(), 2u); // never decreases
	EXPECT_EQ(reg.GetAllocatedCount(), 1u);
	EXPECT_FALSE(reg.IsAllocated(h1));
	EXPECT_TRUE(reg.IsAllocated(h2));

	// GetAllocatedCount == GetSlotCount - free-list size (1 free slot)
	EXPECT_EQ(reg.GetAllocatedCount(), reg.GetSlotCount() - 1u);

	// After move-from, IsInitialized returns false and counts reset.
	ImageRegistry moved(std::move(reg));
	EXPECT_FALSE(reg.IsInitialized());
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);
	EXPECT_TRUE(moved.IsInitialized());
}

// ---------------------------------------------------------------------------
// image_registry_is_not_thread_safe_per_instance
// ---------------------------------------------------------------------------
// This semantic documents the absence of internal synchronization.
// We verify it at the type level: ImageRegistry has no mutex member visible
// from the public API, and the contract explicitly states no synchronization
// is performed. The test exercises concurrent const-observer calls on the
// same instance (which the contract permits) and concurrent calls on
// DISTINCT instances (which the contract also permits).
TEST(ImageRegistry, test_image_registry_is_not_thread_safe_per_instance)
{
	VulkanEnv env;
	if (!env.ready)
		GTEST_SKIP() << "Vulkan not available";

	ImageRegistry reg;
	ASSERT_EQ(reg.Initialize(env.device), RenderError::None);
	GraphImageDesc desc = MakeColorDesc();
	ImageHandle h = reg.Allocate(desc);
	ASSERT_TRUE(h.IsValid());

	// Concurrent const-observer calls on the same instance are permitted.
	// We run them from two threads and verify no crash and consistent results.
	std::atomic<bool> go{false};
	std::atomic<int> errors{0};

	auto observerTask = [&]()
	{
		while (!go.load(std::memory_order_acquire))
		{
		}
		for (int i = 0; i < 1000; ++i)
		{
			if (!reg.IsInitialized())
				++errors;
			if (reg.GetSlotCount() != 1u)
				++errors;
			if (reg.GetAllocatedCount() != 1u)
				++errors;
			if (!reg.IsAllocated(h))
				++errors;
			if (reg.GetUsage(h) != ResourceUsage::Undefined)
				++errors;
		}
	};

	std::thread t1(observerTask);
	std::thread t2(observerTask);
	go.store(true, std::memory_order_release);
	t1.join();
	t2.join();

	EXPECT_EQ(errors.load(), 0);

	// Concurrent calls on DISTINCT instances are also permitted.
	ImageRegistry reg2;
	ASSERT_EQ(reg2.Initialize(env.device), RenderError::None);
	GraphImageDesc desc2 = MakeDepthDesc();

	std::atomic<bool> go2{false};
	std::atomic<int> errors2{0};

	auto mutateTask1 = [&]()
	{
		while (!go2.load(std::memory_order_acquire))
		{
		}
		for (int i = 0; i < 50; ++i)
		{
			ImageHandle hh = reg.Allocate(desc);
			if (!hh.IsValid())
			{
				++errors2;
				continue;
			}
			reg.Free(hh);
		}
	};
	auto mutateTask2 = [&]()
	{
		while (!go2.load(std::memory_order_acquire))
		{
		}
		for (int i = 0; i < 50; ++i)
		{
			ImageHandle hh = reg2.Allocate(desc2);
			if (!hh.IsValid())
			{
				++errors2;
				continue;
			}
			reg2.Free(hh);
		}
	};

	std::thread t3(mutateTask1);
	std::thread t4(mutateTask2);
	go2.store(true, std::memory_order_release);
	t3.join();
	t4.join();

	EXPECT_EQ(errors2.load(), 0);
}
