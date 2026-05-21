#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/resources/Image.h"
#include "virasa/window/Platform.h"
#include "virasa/window/Types.h"
#include "vulkan/vulkan.h"

using namespace virasa;
using namespace virasa::window;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
namespace
{

// Required Vulkan instance extensions for headless testing (none beyond core).
const char* const* GetNoExtensions(uint32_t* count)
{
	*count = 0;
	return nullptr;
}

// Build a RendererConfig suitable for headless tests (no surface, validation
// optional).  We disable validation to avoid needing validation layers in CI.
RendererConfig MakeHeadlessConfig()
{
	RendererConfig cfg;
	cfg.applicationName = "ImageTest";
	cfg.enableValidation = false;
	cfg.requiredInstanceExtensions = nullptr;
	cfg.requiredInstanceExtensionCount = 0;
	return cfg;
}

// Attempt to create a real Instance + Device pair for tests that need Vulkan.
// Returns false if the environment does not support Vulkan (CI without GPU).
struct VulkanContext
{
	Instance instance;
	Device device;
	bool valid = false;

	VulkanContext()
	{
		RendererConfig cfg = MakeHeadlessConfig();
		if (instance.Initialize(cfg) != RenderError::None)
			return;
		// Pass VK_NULL_HANDLE as surface for headless device selection.
		if (device.Initialize(instance, VK_NULL_HANDLE) != RenderError::None)
			return;
		valid = true;
	}
};

// Build a minimal valid ImageConfig for a colour attachment.
ImageConfig MakeColorConfig(uint32_t w = 64, uint32_t h = 64)
{
	ImageConfig cfg;
	cfg.width = w;
	cfg.height = h;
	cfg.format = VK_FORMAT_B8G8R8A8_SRGB;
	cfg.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	cfg.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	cfg.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	cfg.flags = 0;
	return cfg;
}

} // namespace

// ===========================================================================
// image_is_raii_movable_non_copyable
// ===========================================================================
TEST(Image, test_image_is_raii_movable_non_copyable)
{
// Verify static type traits.
STATIC_ASSERT_THAT_IS_NOT_COPY_CONSTRUCTIBLE:
	EXPECT_FALSE((std::is_copy_constructible_v<Image>));
	EXPECT_FALSE((std::is_copy_assignable_v<Image>));
	EXPECT_TRUE((std::is_move_constructible_v<Image>));
	EXPECT_TRUE((std::is_move_assignable_v<Image>));
	EXPECT_TRUE((std::is_default_constructible_v<Image>));
	EXPECT_TRUE((std::is_final_v<Image>));

	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available; skipping move-ownership test.";
	}

	ImageConfig cfg = MakeColorConfig();

	// Initialize source image.
	Image src;
	ASSERT_EQ(src.Initialize(ctx.device, cfg), RenderError::None);
	EXPECT_TRUE(src.IsInitialized());
	VkImage srcHandle = src.GetHandle();
	VkImageView srcView = src.GetView();
	EXPECT_NE(srcHandle, VK_NULL_HANDLE);
	EXPECT_NE(srcView, VK_NULL_HANDLE);

	// Move-construct.
	Image dst(std::move(src));

	// Source must be in default-constructed state.
	EXPECT_FALSE(src.IsInitialized());
	EXPECT_EQ(src.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(src.GetView(), VK_NULL_HANDLE);

	// Destination must own the resources.
	EXPECT_TRUE(dst.IsInitialized());
	EXPECT_EQ(dst.GetHandle(), srcHandle);
	EXPECT_EQ(dst.GetView(), srcView);

	// Move-assign into another image that already owns resources.
	Image dst2;
	ASSERT_EQ(dst2.Initialize(ctx.device, cfg), RenderError::None);
	EXPECT_TRUE(dst2.IsInitialized());

	dst2 = std::move(dst);

	// dst must now be empty.
	EXPECT_FALSE(dst.IsInitialized());
	EXPECT_EQ(dst.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(dst.GetView(), VK_NULL_HANDLE);

	// dst2 must own the resources originally in dst.
	EXPECT_TRUE(dst2.IsInitialized());
	EXPECT_EQ(dst2.GetHandle(), srcHandle);
	EXPECT_EQ(dst2.GetView(), srcView);

	// Destroying a moved-from Image is well-defined (no crash).
	// src and dst go out of scope here; no explicit check needed beyond
	// the absence of a crash / Vulkan validation error.
	ctx.device.WaitIdle();
}

// ===========================================================================
// image_default_constructed_state
// ===========================================================================
TEST(Image, test_image_default_constructed_state)
{
	Image img;

	EXPECT_FALSE(img.IsInitialized());
	EXPECT_EQ(img.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(img.GetView(), VK_NULL_HANDLE);
	// Destroying a default-constructed Image must not crash.
}

// ===========================================================================
// image_config_describes_image_creation_parameters
// ===========================================================================
TEST(Image, test_image_config_describes_image_creation_parameters)
{
	// Default-constructed ImageConfig must have the pinned defaults.
	ImageConfig cfg;
	EXPECT_EQ(cfg.width, 0u);
	EXPECT_EQ(cfg.height, 0u);
	EXPECT_EQ(cfg.format, VK_FORMAT_B8G8R8A8_SRGB);
	EXPECT_EQ(cfg.usage, static_cast<VkImageUsageFlags>(0));
	EXPECT_EQ(cfg.aspect, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
	EXPECT_EQ(cfg.memoryProperties,
		static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	EXPECT_EQ(cfg.flags, static_cast<VkImageCreateFlags>(0));

	// ImageConfig must be copyable, movable, and default-constructible.
	EXPECT_TRUE((std::is_copy_constructible_v<ImageConfig>));
	EXPECT_TRUE((std::is_copy_assignable_v<ImageConfig>));
	EXPECT_TRUE((std::is_move_constructible_v<ImageConfig>));
	EXPECT_TRUE((std::is_move_assignable_v<ImageConfig>));
	EXPECT_TRUE((std::is_default_constructible_v<ImageConfig>));

	// Verify that fields can be set and survive a copy.
	ImageConfig src;
	src.width = 128;
	src.height = 256;
	src.format = VK_FORMAT_D32_SFLOAT;
	src.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	src.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	src.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	src.flags = 0;

	ImageConfig copy = src;
	EXPECT_EQ(copy.width, 128u);
	EXPECT_EQ(copy.height, 256u);
	EXPECT_EQ(copy.format, VK_FORMAT_D32_SFLOAT);
	EXPECT_EQ(copy.usage,
		static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT));
	EXPECT_EQ(copy.aspect, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT));
	EXPECT_EQ(copy.memoryProperties,
		static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	EXPECT_EQ(copy.flags, static_cast<VkImageCreateFlags>(0));
}

// ===========================================================================
// image_initialize_creates_image_memory_and_view
// ===========================================================================
TEST(Image, test_image_initialize_creates_image_memory_and_view)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available; skipping Initialize test.";
	}

	ImageConfig cfg = MakeColorConfig(128, 64);

	Image img;
	RenderError err = img.Initialize(ctx.device, cfg);
	ASSERT_EQ(err, RenderError::None);

	// Post-conditions after successful Initialize.
	EXPECT_TRUE(img.IsInitialized());
	EXPECT_NE(img.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(img.GetView(), VK_NULL_HANDLE);
	EXPECT_EQ(img.GetFormat(), cfg.format);
	EXPECT_EQ(img.GetWidth(), cfg.width);
	EXPECT_EQ(img.GetHeight(), cfg.height);
	VkExtent2D ext = img.GetExtent();
	EXPECT_EQ(ext.width, cfg.width);
	EXPECT_EQ(ext.height, cfg.height);

	// Re-initialization: calling Initialize again must not leak resources and
	// must succeed, updating the cached state.
	ImageConfig cfg2 = MakeColorConfig(32, 32);
	err = img.Initialize(ctx.device, cfg2);
	ASSERT_EQ(err, RenderError::None);
	EXPECT_TRUE(img.IsInitialized());
	EXPECT_NE(img.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(img.GetView(), VK_NULL_HANDLE);
	EXPECT_EQ(img.GetWidth(), 32u);
	EXPECT_EQ(img.GetHeight(), 32u);

	ctx.device.WaitIdle();
}

// ===========================================================================
// image_memory_type_selection
// ===========================================================================
TEST(Image, test_image_memory_type_selection)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available; skipping memory-type-selection test.";
	}

	// Happy path: DEVICE_LOCAL memory — standard for GPU-only attachments.
	{
		ImageConfig cfg = MakeColorConfig(64, 64);
		cfg.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		Image img;
		EXPECT_EQ(img.Initialize(ctx.device, cfg), RenderError::None);
		EXPECT_TRUE(img.IsInitialized());
		ctx.device.WaitIdle();
	}

	// Failure path: request an impossible combination of memory property flags
	// (DEVICE_LOCAL | HOST_VISIBLE | HOST_COHERENT | HOST_CACHED | LAZILY_ALLOCATED
	// | PROTECTED) — no real device exposes all of these simultaneously.
	// If the device happens to support it the test would pass Initialize, which
	// is acceptable; the important assertion is that a truly impossible set
	// returns MemoryAllocFailed.
	{
		VkMemoryPropertyFlags impossible =
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT | VK_MEMORY_PROPERTY_PROTECTED_BIT;

		ImageConfig cfg = MakeColorConfig(64, 64);
		cfg.memoryProperties = impossible;
		Image img;
		RenderError err = img.Initialize(ctx.device, cfg);
		// Either it found a matching type (unlikely) or it failed with
		// MemoryAllocFailed.  Both are valid; we just ensure it doesn't crash
		// and that on failure the image is left not-initialized.
		if (err != RenderError::None)
		{
			EXPECT_EQ(err, RenderError::MemoryAllocFailed);
			EXPECT_FALSE(img.IsInitialized());
			EXPECT_EQ(img.GetHandle(), VK_NULL_HANDLE);
			EXPECT_EQ(img.GetView(), VK_NULL_HANDLE);
		}
	}
}

// ===========================================================================
// image_resize_recreates_at_new_extent
// ===========================================================================
TEST(Image, test_image_resize_recreates_at_new_extent)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available; skipping Resize test.";
	}

	ImageConfig cfg = MakeColorConfig(64, 64);
	Image img;
	ASSERT_EQ(img.Initialize(ctx.device, cfg), RenderError::None);

	VkImage oldHandle = img.GetHandle();
	VkImageView oldView = img.GetView();

	ctx.device.WaitIdle();

	// Resize to a different extent.
	RenderError err = img.Resize(128, 256);
	ASSERT_EQ(err, RenderError::None);

	EXPECT_TRUE(img.IsInitialized());
	EXPECT_NE(img.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(img.GetView(), VK_NULL_HANDLE);
	EXPECT_EQ(img.GetWidth(), 128u);
	EXPECT_EQ(img.GetHeight(), 256u);
	// Format must be preserved.
	EXPECT_EQ(img.GetFormat(), cfg.format);
	VkExtent2D ext = img.GetExtent();
	EXPECT_EQ(ext.width, 128u);
	EXPECT_EQ(ext.height, 256u);
	// Handles should have been recreated (new objects).
	// We can't guarantee they differ (allocator may reuse), but we can verify
	// they are non-null.
	EXPECT_NE(img.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(img.GetView(), VK_NULL_HANDLE);

	// Resize on a default-constructed (never-initialized) Image must return
	// NotInitialized.
	Image uninit;
	EXPECT_EQ(uninit.Resize(64, 64), RenderError::NotInitialized);
	EXPECT_FALSE(uninit.IsInitialized());

	// Resize on a moved-from Image must return NotInitialized.
	Image moved(std::move(img));
	ctx.device.WaitIdle();
	EXPECT_EQ(img.Resize(64, 64), RenderError::NotInitialized);
	EXPECT_FALSE(img.IsInitialized());

	ctx.device.WaitIdle();
}

// ===========================================================================
// image_observers_return_cached_state
// ===========================================================================
TEST(Image, test_image_observers_return_cached_state)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available; skipping observers test.";
	}

	ImageConfig cfg;
	cfg.width = 100;
	cfg.height = 200;
	cfg.format = VK_FORMAT_B8G8R8A8_UNORM;
	cfg.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	cfg.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	cfg.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	cfg.flags = 0;

	Image img;
	ASSERT_EQ(img.Initialize(ctx.device, cfg), RenderError::None);

	// All observers must reflect the config used at Initialize.
	EXPECT_NE(img.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(img.GetView(), VK_NULL_HANDLE);
	EXPECT_EQ(img.GetFormat(), VK_FORMAT_B8G8R8A8_UNORM);
	EXPECT_EQ(img.GetWidth(), 100u);
	EXPECT_EQ(img.GetHeight(), 200u);
	VkExtent2D ext = img.GetExtent();
	EXPECT_EQ(ext.width, 100u);
	EXPECT_EQ(ext.height, 200u);

	// After Resize, dimensions update but format stays.
	ctx.device.WaitIdle();
	ASSERT_EQ(img.Resize(50, 75), RenderError::None);
	EXPECT_EQ(img.GetWidth(), 50u);
	EXPECT_EQ(img.GetHeight(), 75u);
	EXPECT_EQ(img.GetFormat(), VK_FORMAT_B8G8R8A8_UNORM);
	VkExtent2D ext2 = img.GetExtent();
	EXPECT_EQ(ext2.width, 50u);
	EXPECT_EQ(ext2.height, 75u);

	ctx.device.WaitIdle();
}

// ===========================================================================
// image_is_initialized_reflects_owned_handle
// ===========================================================================
TEST(Image, test_image_is_initialized_reflects_owned_handle)
{
	// Default-constructed: not initialized.
	Image img;
	EXPECT_FALSE(img.IsInitialized());
	EXPECT_EQ(img.GetHandle(), VK_NULL_HANDLE);
	// IsInitialized iff GetHandle != VK_NULL_HANDLE.
	EXPECT_EQ(img.IsInitialized(), img.GetHandle() != VK_NULL_HANDLE);

	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available; skipping initialized-state test.";
	}

	ImageConfig cfg = MakeColorConfig();
	ASSERT_EQ(img.Initialize(ctx.device, cfg), RenderError::None);
	EXPECT_TRUE(img.IsInitialized());
	EXPECT_NE(img.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(img.IsInitialized(), img.GetHandle() != VK_NULL_HANDLE);

	// After move-from, not initialized.
	Image dst(std::move(img));
	EXPECT_FALSE(img.IsInitialized());
	EXPECT_EQ(img.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(img.IsInitialized(), img.GetHandle() != VK_NULL_HANDLE);

	// dst is initialized.
	EXPECT_TRUE(dst.IsInitialized());
	EXPECT_NE(dst.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(dst.IsInitialized(), dst.GetHandle() != VK_NULL_HANDLE);

	ctx.device.WaitIdle();
}

// ===========================================================================
// image_is_not_thread_safe_per_instance
// ===========================================================================
// This semantic is a documentation-only contract about the absence of internal
// synchronization.  We verify the compile-time and API-level aspects that are
// observable without introducing data races (which would be UB):
//   - const observers are all noexcept (verifiable at compile time).
//   - The class does not expose any mutex or atomic member in its public API.
TEST(Image, test_image_is_not_thread_safe_per_instance)
{
	// All const observers must be noexcept.
	EXPECT_TRUE(noexcept(std::declval<const Image&>().GetHandle()));
	EXPECT_TRUE(noexcept(std::declval<const Image&>().GetView()));
	EXPECT_TRUE(noexcept(std::declval<const Image&>().GetFormat()));
	EXPECT_TRUE(noexcept(std::declval<const Image&>().GetWidth()));
	EXPECT_TRUE(noexcept(std::declval<const Image&>().GetHeight()));
	EXPECT_TRUE(noexcept(std::declval<const Image&>().GetExtent()));
	EXPECT_TRUE(noexcept(std::declval<const Image&>().IsInitialized()));

	// Multiple const observers may be called sequentially on the same Image
	// without any synchronization — this is the only safe concurrent pattern
	// the contract pins.
	Image img;
	EXPECT_EQ(img.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(img.GetView(), VK_NULL_HANDLE);
	EXPECT_FALSE(img.IsInitialized());
	// Calling them again (simulating two "threads" sequentially) must be stable.
	EXPECT_EQ(img.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(img.GetView(), VK_NULL_HANDLE);
	EXPECT_FALSE(img.IsInitialized());
}
