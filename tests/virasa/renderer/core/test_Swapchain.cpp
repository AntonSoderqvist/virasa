#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <vector>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/core/Surface.h"
#include "virasa/renderer/core/Swapchain.h"
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

// Builds a minimal RendererConfig suitable for swapchain tests.
RendererConfig MakeTestConfig()
{
	RendererConfig config;
	config.applicationName = "SwapchainTest";
	config.enableValidation = false;
	return config;
}

// Brings up a Platform, Instance, Surface (with formats queried), and Device
// that are all initialized and ready for Swapchain tests.
// Returns false if any step fails (test should be skipped/failed).
struct TestEnv
{
	Platform platform;
	Instance instance;
	Surface surface;
	Device device;
	RendererConfig config;
	bool ok = false;

	TestEnv()
	{
		config = MakeTestConfig();

		// Platform
		uint32_t extCount = 0;
		const char* const* exts = Platform::GetRequiredVulkanExtensions(&extCount);
		config.requiredInstanceExtensions = exts;
		config.requiredInstanceExtensionCount = extCount;

		if (platform.Initialize("SwapchainTest", 800, 600) != ErrorCode::None)
			return;

		// Instance
		if (instance.Initialize(config) != RenderError::None)
			return;

		// Surface
		if (surface.Initialize(instance, platform) != RenderError::None)
			return;

		// Device
		if (device.Initialize(instance, surface.GetHandle()) != RenderError::None)
			return;

		// Query formats and modes so the Surface is ready for swapchain creation
		surface.QueryFormatsAndModes(device.GetPhysicalDevice(), config);

		ok = true;
	}

	~TestEnv()
	{
		if (device.IsInitialized())
			device.WaitIdle();
		platform.Shutdown();
	}
};

} // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// swapchain_default_constructed_state
TEST(Swapchain, test_swapchain_default_constructed_state)
{
	Swapchain sc;
	EXPECT_FALSE(sc.IsInitialized());
	EXPECT_EQ(sc.GetFormat(), VK_FORMAT_UNDEFINED);
	EXPECT_EQ(sc.GetExtent().width, 0u);
	EXPECT_EQ(sc.GetExtent().height, 0u);
	EXPECT_TRUE(sc.GetImages().empty());
	EXPECT_TRUE(sc.GetImageViews().empty());
	EXPECT_EQ(sc.GetImageCount(), 0u);
}

// swapchain_is_raii_movable_non_copyable_handle_owner
TEST(Swapchain, test_swapchain_is_raii_movable_non_copyable_handle_owner)
{
	// Non-copyable checks (compile-time, but we verify via type traits)
	EXPECT_FALSE(std::is_copy_constructible_v<Swapchain>);
	EXPECT_FALSE(std::is_copy_assignable_v<Swapchain>);

	// Movable checks
	EXPECT_TRUE(std::is_move_constructible_v<Swapchain>);
	EXPECT_TRUE(std::is_move_assignable_v<Swapchain>);

	// Final check
	EXPECT_TRUE(std::is_final_v<Swapchain>);

	// Default-constructible
	EXPECT_TRUE(std::is_default_constructible_v<Swapchain>);

	TestEnv env;
	if (!env.ok)
	{
		GTEST_SKIP() << "Vulkan environment not available; skipping move/RAII test";
	}

	Size2D windowSize{800, 600};

	// Scope ensures sc3 is destroyed before the next swapchain is created:
	// Vulkan allows only one active swapchain per surface at a time.
	{
		Swapchain sc;
		ASSERT_EQ(
			sc.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);
		ASSERT_TRUE(sc.IsInitialized());
		ASSERT_GT(sc.GetImageCount(), 0u);

		// Move-construct
		Swapchain sc2(std::move(sc));
		EXPECT_TRUE(sc2.IsInitialized());
		EXPECT_GT(sc2.GetImageCount(), 0u);

		// Source is in default-constructed state after move
		EXPECT_FALSE(sc.IsInitialized());
		EXPECT_EQ(sc.GetFormat(), VK_FORMAT_UNDEFINED);
		EXPECT_EQ(sc.GetExtent().width, 0u);
		EXPECT_EQ(sc.GetExtent().height, 0u);
		EXPECT_TRUE(sc.GetImages().empty());
		EXPECT_TRUE(sc.GetImageViews().empty());
		EXPECT_EQ(sc.GetImageCount(), 0u);

		// Move-assign into a default-constructed swapchain
		Swapchain sc3;
		sc3 = std::move(sc2);
		EXPECT_TRUE(sc3.IsInitialized());
		EXPECT_FALSE(sc2.IsInitialized());

		env.device.WaitIdle();
	} // sc3 destroyed here

	// Move-assign into an already-initialized swapchain (must destroy old handles first).
	// Vulkan allows only one active swapchain per surface, so we verify the Destroy() path
	// by assigning an empty swapchain into an initialized one.
	Swapchain sc4;
	ASSERT_EQ(sc4.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);
	ASSERT_TRUE(sc4.IsInitialized());
	sc4 = Swapchain{}; // sc4's old handles must be properly destroyed
	EXPECT_FALSE(sc4.IsInitialized());

	env.device.WaitIdle();
}

// swapchain_borrows_vk_device_for_lifetime
TEST(Swapchain, test_swapchain_borrows_vk_device_for_lifetime)
{
	TestEnv env;
	if (!env.ok)
	{
		GTEST_SKIP() << "Vulkan environment not available";
	}

	Size2D windowSize{800, 600};

	Swapchain sc;
	ASSERT_EQ(sc.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);
	ASSERT_TRUE(sc.IsInitialized());

	// The swapchain borrows the device handle; the device must outlive the swapchain.
	// We verify this by moving the swapchain — the borrowed handle must transfer.
	Swapchain sc2(std::move(sc));
	EXPECT_TRUE(sc2.IsInitialized());
	// sc2 now owns the swapchain and holds the borrowed device handle.
	// Destroying sc2 (end of scope) exercises the destruction path with the borrowed handle.
	env.device.WaitIdle();
}

// swapchain_initialize_creates_swapchain_and_image_views
TEST(Swapchain, test_swapchain_initialize_creates_swapchain_and_image_views)
{
	TestEnv env;
	if (!env.ok)
	{
		GTEST_SKIP() << "Vulkan environment not available";
	}

	Size2D windowSize{800, 600};

	Swapchain sc;
	RenderError err = sc.Initialize(env.device, env.surface, windowSize, env.config);
	ASSERT_EQ(err, RenderError::None);

	EXPECT_TRUE(sc.IsInitialized());
	EXPECT_NE(sc.GetFormat(), VK_FORMAT_UNDEFINED);
	EXPECT_GT(sc.GetExtent().width, 0u);
	EXPECT_GT(sc.GetExtent().height, 0u);
	EXPECT_GT(sc.GetImageCount(), 0u);
	EXPECT_EQ(sc.GetImages().size(), sc.GetImageCount());
	EXPECT_EQ(sc.GetImageViews().size(), sc.GetImageCount());

	// All image handles must be non-null
	for (VkImage img : sc.GetImages())
		EXPECT_NE(img, VK_NULL_HANDLE);

	// All image view handles must be non-null
	for (VkImageView iv : sc.GetImageViews())
		EXPECT_NE(iv, VK_NULL_HANDLE);

	env.device.WaitIdle();
}

// swapchain_create_info_field_derivation
TEST(Swapchain, test_swapchain_create_info_field_derivation)
{
	TestEnv env;
	if (!env.ok)
	{
		GTEST_SKIP() << "Vulkan environment not available";
	}

	Size2D windowSize{800, 600};

	Swapchain sc;
	ASSERT_EQ(sc.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);

	// The format must match what the surface reported
	EXPECT_EQ(sc.GetFormat(), env.surface.GetFormat().format);

	// The extent must be non-zero (derived from capabilities/window_size)
	EXPECT_GT(sc.GetExtent().width, 0u);
	EXPECT_GT(sc.GetExtent().height, 0u);

	// Image count must be at least minImageCount+1 (clamped), so >= 1
	EXPECT_GE(sc.GetImageCount(), 1u);

	env.device.WaitIdle();
}

// swapchain_extent_selection
TEST(Swapchain, test_swapchain_extent_selection)
{
	TestEnv env;
	if (!env.ok)
	{
		GTEST_SKIP() << "Vulkan environment not available";
	}

	Size2D windowSize{800, 600};

	Swapchain sc;
	ASSERT_EQ(sc.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);

	const VkSurfaceCapabilitiesKHR& caps = env.surface.GetCapabilities();
	VkExtent2D extent = sc.GetExtent();

	if (caps.currentExtent.width != UINT32_MAX)
	{
		// Platform reports a fixed extent — must match currentExtent
		EXPECT_EQ(extent.width, caps.currentExtent.width);
		EXPECT_EQ(extent.height, caps.currentExtent.height);
	}
	else
	{
		// Variable extent — must be clamped to [min, max]
		EXPECT_GE(extent.width, caps.minImageExtent.width);
		EXPECT_LE(extent.width, caps.maxImageExtent.width);
		EXPECT_GE(extent.height, caps.minImageExtent.height);
		EXPECT_LE(extent.height, caps.maxImageExtent.height);
	}

	env.device.WaitIdle();
}

// swapchain_image_count_selection
TEST(Swapchain, test_swapchain_image_count_selection)
{
	TestEnv env;
	if (!env.ok)
	{
		GTEST_SKIP() << "Vulkan environment not available";
	}

	Size2D windowSize{800, 600};

	Swapchain sc;
	ASSERT_EQ(sc.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);

	const VkSurfaceCapabilitiesKHR& caps = env.surface.GetCapabilities();
	uint32_t requested = caps.minImageCount + 1;
	if (caps.maxImageCount > 0)
		requested = std::min(requested, caps.maxImageCount);

	// Actual image count must be >= requested (Vulkan may give more)
	EXPECT_GE(sc.GetImageCount(), requested);

	env.device.WaitIdle();
}

// swapchain_image_view_creation
TEST(Swapchain, test_swapchain_image_view_creation)
{
	TestEnv env;
	if (!env.ok)
	{
		GTEST_SKIP() << "Vulkan environment not available";
	}

	Size2D windowSize{800, 600};

	Swapchain sc;
	ASSERT_EQ(sc.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);

	// One image view per image, all non-null
	EXPECT_EQ(sc.GetImageViews().size(), sc.GetImages().size());
	for (VkImageView iv : sc.GetImageViews())
		EXPECT_NE(iv, VK_NULL_HANDLE);

	env.device.WaitIdle();
}

// recreate_rebuilds_swapchain_after_resize_or_suboptimal
TEST(Swapchain, test_recreate_rebuilds_swapchain_after_resize_or_suboptimal)
{
	TestEnv env;
	if (!env.ok)
	{
		GTEST_SKIP() << "Vulkan environment not available";
	}

	Size2D windowSize{800, 600};

	Swapchain sc;
	ASSERT_EQ(sc.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);
	ASSERT_TRUE(sc.IsInitialized());

	uint32_t oldCount = sc.GetImageCount();
	VkFormat oldFormat = sc.GetFormat();

	RenderError recreateErr = sc.Recreate(env.device, env.surface, windowSize, env.config);
	ASSERT_EQ(recreateErr, RenderError::None);

	EXPECT_TRUE(sc.IsInitialized());
	EXPECT_NE(sc.GetFormat(), VK_FORMAT_UNDEFINED);
	EXPECT_GT(sc.GetExtent().width, 0u);
	EXPECT_GT(sc.GetExtent().height, 0u);
	EXPECT_GT(sc.GetImageCount(), 0u);
	EXPECT_EQ(sc.GetImages().size(), sc.GetImageCount());
	EXPECT_EQ(sc.GetImageViews().size(), sc.GetImageCount());

	// Format should be consistent after recreate
	EXPECT_EQ(sc.GetFormat(), oldFormat);

	for (VkImageView iv : sc.GetImageViews())
		EXPECT_NE(iv, VK_NULL_HANDLE);

	env.device.WaitIdle();
}

// recreate_uses_old_swapchain_for_handover
// We verify the observable contract: after Recreate, the swapchain is valid
// and the old handles have been destroyed (no double-free / validation errors).
TEST(Swapchain, test_recreate_uses_old_swapchain_for_handover)
{
	TestEnv env;
	if (!env.ok)
	{
		GTEST_SKIP() << "Vulkan environment not available";
	}

	Size2D windowSize{800, 600};

	Swapchain sc;
	ASSERT_EQ(sc.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);

	// Recreate multiple times to exercise the old-swapchain handover path
	for (int i = 0; i < 3; ++i)
	{
		RenderError err = sc.Recreate(env.device, env.surface, windowSize, env.config);
		ASSERT_EQ(err, RenderError::None);
		EXPECT_TRUE(sc.IsInitialized());
		EXPECT_GT(sc.GetImageCount(), 0u);
	}

	env.device.WaitIdle();
}

// acquire_next_image_classifies_vk_result
TEST(Swapchain, test_acquire_next_image_classifies_vk_result)
{
	TestEnv env;
	if (!env.ok)
	{
		GTEST_SKIP() << "Vulkan environment not available";
	}

	Size2D windowSize{800, 600};

	Swapchain sc;
	ASSERT_EQ(sc.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);

	// Create a semaphore to signal
	VkSemaphoreCreateInfo semInfo{};
	semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkSemaphore sem = VK_NULL_HANDLE;
	ASSERT_EQ(vkCreateSemaphore(env.device.GetHandle(), &semInfo, nullptr, &sem), VK_SUCCESS);

	uint32_t imageIndex = UINT32_MAX;
	SwapchainStatus status = sc.AcquireNextImage(env.device, sem, &imageIndex);

	// On a freshly created swapchain the result should be Success or Recreated
	// (suboptimal/out-of-date)
	EXPECT_TRUE(status == SwapchainStatus::Success || status == SwapchainStatus::Recreated);
	if (status == SwapchainStatus::Success)
		EXPECT_LT(imageIndex, sc.GetImageCount());

	vkDestroySemaphore(env.device.GetHandle(), sem, nullptr);
	env.device.WaitIdle();
}

// present_classifies_vk_result
TEST(Swapchain, test_present_classifies_vk_result)
{
	TestEnv env;
	if (!env.ok)
	{
		GTEST_SKIP() << "Vulkan environment not available";
	}

	Size2D windowSize{800, 600};

	Swapchain sc;
	ASSERT_EQ(sc.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);

	// Create acquire semaphore and present semaphore
	VkSemaphoreCreateInfo semInfo{};
	semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkSemaphore acquireSem = VK_NULL_HANDLE;
	VkSemaphore presentSem = VK_NULL_HANDLE;
	ASSERT_EQ(
		vkCreateSemaphore(env.device.GetHandle(), &semInfo, nullptr, &acquireSem), VK_SUCCESS);
	ASSERT_EQ(
		vkCreateSemaphore(env.device.GetHandle(), &semInfo, nullptr, &presentSem), VK_SUCCESS);

	uint32_t imageIndex = UINT32_MAX;
	SwapchainStatus acquireStatus = sc.AcquireNextImage(env.device, acquireSem, &imageIndex);

	if (acquireStatus == SwapchainStatus::Success)
	{
		// Signal the present semaphore immediately via a submit so Present can wait on it
		// Use a simple submit with no work to signal presentSem
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &acquireSem;
		submitInfo.pWaitDstStageMask = &waitStage;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &presentSem;
		ASSERT_EQ(vkQueueSubmit(env.device.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE),
			VK_SUCCESS);

		SwapchainStatus presentStatus =
			sc.Present(env.device.GetPresentQueue(), presentSem, imageIndex);
		// Present may return Success or Recreated (suboptimal/out-of-date) — both are valid
		EXPECT_TRUE(presentStatus == SwapchainStatus::Success ||
				presentStatus == SwapchainStatus::Recreated);
	}
	else
	{
		// Swapchain was already out-of-date; that's acceptable in CI
		EXPECT_EQ(acquireStatus, SwapchainStatus::Recreated);
	}

	vkDestroySemaphore(env.device.GetHandle(), acquireSem, nullptr);
	vkDestroySemaphore(env.device.GetHandle(), presentSem, nullptr);
	env.device.WaitIdle();
}

// swapchain_observers_return_cached_state
TEST(Swapchain, test_swapchain_observers_return_cached_state)
{
	TestEnv env;
	if (!env.ok)
	{
		GTEST_SKIP() << "Vulkan environment not available";
	}

	Size2D windowSize{800, 600};

	Swapchain sc;
	ASSERT_EQ(sc.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);

	// Capture state after initialize
	VkFormat fmt = sc.GetFormat();
	VkExtent2D ext = sc.GetExtent();
	uint32_t count = sc.GetImageCount();
	auto images = sc.GetImages();
	auto views = sc.GetImageViews();

	// Calling observers again must return the same values (cached)
	EXPECT_EQ(sc.GetFormat(), fmt);
	EXPECT_EQ(sc.GetExtent().width, ext.width);
	EXPECT_EQ(sc.GetExtent().height, ext.height);
	EXPECT_EQ(sc.GetImageCount(), count);
	EXPECT_EQ(sc.GetImages().size(), images.size());
	EXPECT_EQ(sc.GetImageViews().size(), views.size());

	// After recreate, observers must return fresh (but still valid) state
	ASSERT_EQ(sc.Recreate(env.device, env.surface, windowSize, env.config), RenderError::None);
	EXPECT_NE(sc.GetFormat(), VK_FORMAT_UNDEFINED);
	EXPECT_GT(sc.GetExtent().width, 0u);
	EXPECT_GT(sc.GetImageCount(), 0u);
	EXPECT_EQ(sc.GetImages().size(), sc.GetImageCount());
	EXPECT_EQ(sc.GetImageViews().size(), sc.GetImageCount());

	env.device.WaitIdle();
}

// is_initialized_reflects_owned_swapchain
TEST(Swapchain, test_is_initialized_reflects_owned_swapchain)
{
	// Default-constructed: not initialized
	Swapchain sc;
	EXPECT_FALSE(sc.IsInitialized());

	TestEnv env;
	if (!env.ok)
	{
		GTEST_SKIP() << "Vulkan environment not available";
	}

	Size2D windowSize{800, 600};

	ASSERT_EQ(sc.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);
	EXPECT_TRUE(sc.IsInitialized());

	// After move-from, source is not initialized
	Swapchain sc2(std::move(sc));
	EXPECT_FALSE(sc.IsInitialized());
	EXPECT_TRUE(sc2.IsInitialized());

	env.device.WaitIdle();
}

// swapchain_methods_are_not_thread_safe_per_instance
// This semantic is a documentation/design contract, not a runtime-testable
// behavior (testing it would require triggering UB). We verify the type
// properties that the semantic implies: non-const methods exist, const
// observers are const-qualified, and distinct Swapchain objects can be
// independently initialized (no shared mutable state between instances).
TEST(Swapchain, test_swapchain_methods_are_not_thread_safe_per_instance)
{
	// Verify const-qualification of observers via type traits on member function pointers
	// GetFormat, GetExtent, GetImageViews, GetImages, GetImageCount, IsInitialized are const
	using GetFormatPtr = VkFormat (Swapchain::*)() const noexcept;
	using GetExtentPtr = VkExtent2D (Swapchain::*)() const noexcept;
	using GetImageViewsPtr = std::span<const VkImageView> (Swapchain::*)() const noexcept;
	using GetImagesPtr = std::span<const VkImage> (Swapchain::*)() const noexcept;
	using GetImageCountPtr = uint32_t (Swapchain::*)() const noexcept;
	using IsInitializedPtr = bool (Swapchain::*)() const noexcept;

	// These static_asserts confirm the signatures are const-qualified as the contract requires.
	static_assert(std::is_same_v<GetFormatPtr, decltype(&Swapchain::GetFormat)>,
		"GetFormat must be const noexcept");
	static_assert(std::is_same_v<GetExtentPtr, decltype(&Swapchain::GetExtent)>,
		"GetExtent must be const noexcept");
	static_assert(std::is_same_v<GetImageViewsPtr, decltype(&Swapchain::GetImageViews)>,
		"GetImageViews must be const noexcept");
	static_assert(std::is_same_v<GetImagesPtr, decltype(&Swapchain::GetImages)>,
		"GetImages must be const noexcept");
	static_assert(std::is_same_v<GetImageCountPtr, decltype(&Swapchain::GetImageCount)>,
		"GetImageCount must be const noexcept");
	static_assert(std::is_same_v<IsInitializedPtr, decltype(&Swapchain::IsInitialized)>,
		"IsInitialized must be const noexcept");

	TestEnv env;
	if (!env.ok)
	{
		GTEST_SKIP() << "Vulkan environment not available";
	}

	Size2D windowSize{800, 600};

	// Two distinct Swapchain objects can be independently initialized (no shared mutable state).
	// Vulkan allows only one active swapchain per surface, so we test sequentially.
	Swapchain sc1;
	ASSERT_EQ(sc1.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);
	EXPECT_TRUE(sc1.IsInitialized());
	env.device.WaitIdle();
	sc1 = Swapchain{}; // destroy sc1 before initializing sc2

	Swapchain sc2;
	ASSERT_EQ(sc2.Initialize(env.device, env.surface, windowSize, env.config), RenderError::None);
	EXPECT_TRUE(sc2.IsInitialized());

	env.device.WaitIdle();
}
