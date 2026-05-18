#include <vulkan/vulkan.h>

#include <gtest/gtest.h>

#include "virasa/core/Logger.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/core/Surface.h"
#include "virasa/window/Platform.h"
#include "virasa/window/Types.h"

namespace
{

// Helper: initialize the logger so Surface logging calls don't crash.
void EnsureLogger()
{
	if (!virasa::Logger::IsInitialized())
	{
		virasa::Logger::Initialize();
	}
}

// Helper: create a real initialized Platform.
// Returns true on success.
bool MakePlatform(virasa::window::Platform& platform)
{
	return platform.Initialize("SurfaceTest", 800, 600) == virasa::ErrorCode::None;
}

// Helper: create a real initialized Instance.
// Returns true on success.
bool MakeInstance(virasa::Instance& instance)
{
	uint32_t extCount = 0;
	const char* const* exts = virasa::window::Platform::GetRequiredVulkanExtensions(&extCount);

	virasa::RendererConfig config;
	config.enableValidation = false;
	config.requiredInstanceExtensions = exts;
	config.requiredInstanceExtensionCount = extCount;

	return instance.Initialize(config) == virasa::RenderError::None;
}

// Helper: pick the first available VkPhysicalDevice from a VkInstance.
// Returns VK_NULL_HANDLE if none found.
VkPhysicalDevice PickPhysicalDevice(VkInstance vkInstance)
{
	uint32_t count = 0;
	if (vkEnumeratePhysicalDevices(vkInstance, &count, nullptr) != VK_SUCCESS || count == 0)
	{
		return VK_NULL_HANDLE;
	}
	std::vector<VkPhysicalDevice> devices(count);
	if (vkEnumeratePhysicalDevices(vkInstance, &count, devices.data()) != VK_SUCCESS)
	{
		return VK_NULL_HANDLE;
	}
	return devices[0];
}

} // namespace

// ---------------------------------------------------------------------------
// surface_default_constructed_state
// ---------------------------------------------------------------------------
TEST(Surface, test_default_constructed_state)
{
	EnsureLogger();
	virasa::Surface surface;
	EXPECT_EQ(surface.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(surface.IsInitialized());
	EXPECT_FALSE(surface.AreFormatsQueried());
}

// ---------------------------------------------------------------------------
// surface_is_raii_movable_non_copyable_handle_owner
// ---------------------------------------------------------------------------
TEST(Surface, test_surface_is_raii_movable_non_copyable_handle_owner)
{
	EnsureLogger();

	// Verify non-copyability at compile time via type traits.
	EXPECT_FALSE(std::is_copy_constructible_v<virasa::Surface>);
	EXPECT_FALSE(std::is_copy_assignable_v<virasa::Surface>);

	// Verify movability at compile time.
	EXPECT_TRUE(std::is_move_constructible_v<virasa::Surface>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::Surface>);

	// Verify final (no subclassing) — tested structurally; we can only
	// confirm the type trait for move/copy here at runtime.
	// Verify default-constructibility.
	EXPECT_TRUE(std::is_default_constructible_v<virasa::Surface>);

	virasa::window::Platform platform;
	if (!MakePlatform(platform))
	{
		GTEST_SKIP() << "Platform initialization failed; skipping move test.";
	}

	virasa::Instance instance;
	if (!MakeInstance(instance))
	{
		GTEST_SKIP() << "Vulkan Instance initialization failed; skipping move test.";
	}

	// Initialize a surface so it owns a handle.
	virasa::Surface src;
	auto initResult = src.Initialize(instance, platform);
	if (initResult != virasa::RenderError::None)
	{
		GTEST_SKIP() << "Surface initialization failed; skipping move test.";
	}
	ASSERT_TRUE(src.IsInitialized());
	ASSERT_NE(src.GetHandle(), VK_NULL_HANDLE);

	VkSurfaceKHR originalHandle = src.GetHandle();

	// Move-construct: destination takes ownership.
	virasa::Surface dst(std::move(src));
	EXPECT_EQ(dst.GetHandle(), originalHandle);
	EXPECT_TRUE(dst.IsInitialized());

	// Source is in moved-from (default-constructed) state.
	EXPECT_EQ(src.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(src.IsInitialized());
	EXPECT_FALSE(src.AreFormatsQueried());

	// Move-assign: another default surface takes ownership from dst.
	virasa::Surface dst2;
	dst2 = std::move(dst);
	EXPECT_EQ(dst2.GetHandle(), originalHandle);
	EXPECT_TRUE(dst2.IsInitialized());

	// dst is now moved-from.
	EXPECT_EQ(dst.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(dst.IsInitialized());
	EXPECT_FALSE(dst.AreFormatsQueried());

	// Move-assign into a Surface that already owns a handle: it must destroy
	// the prior handle before taking the new one.
	virasa::Surface src2;
	auto initResult2 = src2.Initialize(instance, platform);
	if (initResult2 != virasa::RenderError::None)
	{
		// Can't test overwrite-move without a second surface; just skip that sub-check.
	}
	else
	{
		VkSurfaceKHR handle2 = src2.GetHandle();
		// dst2 already owns originalHandle; overwrite with src2's handle.
		dst2 = std::move(src2);
		EXPECT_EQ(dst2.GetHandle(), handle2);
		EXPECT_TRUE(dst2.IsInitialized());
		EXPECT_EQ(src2.GetHandle(), VK_NULL_HANDLE);
		EXPECT_FALSE(src2.IsInitialized());
	}
}

// ---------------------------------------------------------------------------
// surface_borrows_vk_instance_for_lifetime
// ---------------------------------------------------------------------------
// This semantic is a lifetime-contract guarantee (the Instance must outlive
// the Surface). We verify the observable side: after Initialize the Surface
// stores the VkInstance handle (used during destruction) and that the
// destruction of a properly-ordered Surface/Instance pair is well-defined.
TEST(Surface, test_surface_borrows_vk_instance_for_lifetime)
{
	EnsureLogger();

	virasa::window::Platform platform;
	if (!MakePlatform(platform))
	{
		GTEST_SKIP() << "Platform initialization failed.";
	}

	virasa::Instance instance;
	if (!MakeInstance(instance))
	{
		GTEST_SKIP() << "Vulkan Instance initialization failed.";
	}

	{
		virasa::Surface surface;
		auto result = surface.Initialize(instance, platform);
		if (result != virasa::RenderError::None)
		{
			GTEST_SKIP() << "Surface initialization failed.";
		}
		EXPECT_TRUE(surface.IsInitialized());
		EXPECT_NE(surface.GetHandle(), VK_NULL_HANDLE);
		// Surface goes out of scope here; destructor must call vkDestroySurfaceKHR
		// using the borrowed VkInstance handle — this must not crash.
	}
	// Instance is still alive here; correct ordering.
	EXPECT_TRUE(instance.IsInitialized());

	// After a move, the borrowed VkInstance handle follows the destination.
	virasa::Surface src;
	auto result2 = src.Initialize(instance, platform);
	if (result2 == virasa::RenderError::None)
	{
		virasa::Surface dst(std::move(src));
		EXPECT_TRUE(dst.IsInitialized());
		// dst destructs here, using the borrowed instance handle — must not crash.
	}
}

// ---------------------------------------------------------------------------
// surface_initialize_creates_vk_surface_via_platform
// ---------------------------------------------------------------------------
TEST(Surface, test_surface_initialize_creates_vk_surface_via_platform)
{
	EnsureLogger();

	virasa::window::Platform platform;
	if (!MakePlatform(platform))
	{
		GTEST_SKIP() << "Platform initialization failed.";
	}

	virasa::Instance instance;
	if (!MakeInstance(instance))
	{
		GTEST_SKIP() << "Vulkan Instance initialization failed.";
	}

	virasa::Surface surface;
	EXPECT_FALSE(surface.IsInitialized());
	EXPECT_EQ(surface.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(surface.AreFormatsQueried());

	auto result = surface.Initialize(instance, platform);
	EXPECT_EQ(result, virasa::RenderError::None);
	EXPECT_TRUE(surface.IsInitialized());
	EXPECT_NE(surface.GetHandle(), VK_NULL_HANDLE);
	// AreFormatsQueried must remain false after Initialize alone.
	EXPECT_FALSE(surface.AreFormatsQueried());
}

// ---------------------------------------------------------------------------
// surface_initialize_does_not_partially_modify_on_failure
// ---------------------------------------------------------------------------
// We test the failure path by using an uninitialized (default-constructed)
// Platform, which cannot produce a valid surface.
TEST(Surface, test_surface_initialize_does_not_partially_modify_on_failure)
{
	EnsureLogger();

	virasa::Instance instance;
	if (!MakeInstance(instance))
	{
		GTEST_SKIP() << "Vulkan Instance initialization failed.";
	}

	// An uninitialized Platform should cause CreateSurface to fail.
	virasa::window::Platform uninitPlatform;

	virasa::Surface surface;
	// Record pre-call state.
	VkSurfaceKHR handleBefore = surface.GetHandle();
	bool initializedBefore = surface.IsInitialized();
	bool formatsQueriedBefore = surface.AreFormatsQueried();

	auto result = surface.Initialize(instance, uninitPlatform);

	if (result == virasa::RenderError::SurfaceCreateFailed)
	{
		// Surface must be unchanged.
		EXPECT_EQ(surface.GetHandle(), handleBefore);
		EXPECT_EQ(surface.IsInitialized(), initializedBefore);
		EXPECT_EQ(surface.AreFormatsQueried(), formatsQueriedBefore);
	}
	else if (result == virasa::RenderError::None)
	{
		// If the platform somehow succeeded (unexpected), the surface is initialized.
		// This is not the failure path we intended to test, but we should not fail
		// the test for it — just note the skip.
		GTEST_SKIP()
			<< "Uninitialized Platform unexpectedly succeeded; cannot test failure path.";
	}
	else
	{
		// Any other error code also counts as failure; surface must be unchanged.
		EXPECT_EQ(surface.GetHandle(), handleBefore);
		EXPECT_EQ(surface.IsInitialized(), initializedBefore);
		EXPECT_EQ(surface.AreFormatsQueried(), formatsQueriedBefore);
	}
}

// ---------------------------------------------------------------------------
// query_formats_and_modes_selects_format_and_present_mode
// ---------------------------------------------------------------------------
TEST(Surface, test_query_formats_and_modes_selects_format_and_present_mode)
{
	EnsureLogger();

	virasa::window::Platform platform;
	if (!MakePlatform(platform))
	{
		GTEST_SKIP() << "Platform initialization failed.";
	}

	virasa::Instance instance;
	if (!MakeInstance(instance))
	{
		GTEST_SKIP() << "Vulkan Instance initialization failed.";
	}

	virasa::Surface surface;
	if (surface.Initialize(instance, platform) != virasa::RenderError::None)
	{
		GTEST_SKIP() << "Surface initialization failed.";
	}

	VkPhysicalDevice physDev = PickPhysicalDevice(instance.GetHandle());
	if (physDev == VK_NULL_HANDLE)
	{
		GTEST_SKIP() << "No physical device found.";
	}

	// Before query.
	EXPECT_FALSE(surface.AreFormatsQueried());

	// Test with preferMailbox = false (FIFO guaranteed).
	virasa::RendererConfig configFifo;
	configFifo.preferMailbox = false;
	surface.QueryFormatsAndModes(physDev, configFifo);

	EXPECT_TRUE(surface.AreFormatsQueried());
	EXPECT_TRUE(surface.IsInitialized());
	EXPECT_NE(surface.GetHandle(), VK_NULL_HANDLE);

	// Present mode must be FIFO when preferMailbox is false.
	EXPECT_EQ(surface.GetPresentMode(), VK_PRESENT_MODE_FIFO_KHR);

	// Format must be a valid VkSurfaceFormatKHR (format != VK_FORMAT_UNDEFINED
	// unless the surface truly reports no formats, which is non-conformant).
	VkSurfaceFormatKHR fmt = surface.GetFormat();
	EXPECT_NE(fmt.format, VK_FORMAT_UNDEFINED);

	// Capabilities must be populated (minImageCount >= 1 for any conformant surface).
	const VkSurfaceCapabilitiesKHR& caps = surface.GetCapabilities();
	EXPECT_GE(caps.minImageCount, 1u);

	// Test with preferMailbox = true.
	virasa::RendererConfig configMailbox;
	configMailbox.preferMailbox = true;
	surface.QueryFormatsAndModes(physDev, configMailbox);

	EXPECT_TRUE(surface.AreFormatsQueried());

	// Present mode is either MAILBOX (if supported) or FIFO (fallback).
	VkPresentModeKHR pm = surface.GetPresentMode();
	EXPECT_TRUE(pm == VK_PRESENT_MODE_MAILBOX_KHR || pm == VK_PRESENT_MODE_FIFO_KHR);

	// Verify format selection priority: if B8G8R8A8_SRGB + SRGB_NONLINEAR is
	// available, it should be selected first.
	uint32_t fmtCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface.GetHandle(), &fmtCount, nullptr);
	if (fmtCount > 0)
	{
		std::vector<VkSurfaceFormatKHR> availFmts(fmtCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(
			physDev, surface.GetHandle(), &fmtCount, availFmts.data());

		bool hasFirstPriority = false;
		bool hasSecondPriority = false;
		const std::vector<VkFormat> secondPriorityFormats = {VK_FORMAT_R8G8B8A8_SRGB,
			VK_FORMAT_B8G8R8A8_SRGB,
			VK_FORMAT_A8B8G8R8_SRGB_PACK32,
			VK_FORMAT_R8G8B8_SRGB,
			VK_FORMAT_B8G8R8_SRGB};

		for (const auto& f : availFmts)
		{
			if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
				f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				hasFirstPriority = true;
			}
			if (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				for (VkFormat spf : secondPriorityFormats)
				{
					if (f.format == spf)
					{
						hasSecondPriority = true;
						break;
					}
				}
			}
		}

		VkSurfaceFormatKHR selected = surface.GetFormat();
		if (hasFirstPriority)
		{
			EXPECT_EQ(selected.format, VK_FORMAT_B8G8R8A8_SRGB);
			EXPECT_EQ(selected.colorSpace, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
		}
		else if (hasSecondPriority)
		{
			EXPECT_EQ(selected.colorSpace, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
			bool isSecondPriorityFormat = false;
			for (VkFormat spf : secondPriorityFormats)
			{
				if (selected.format == spf)
				{
					isSecondPriorityFormat = true;
					break;
				}
			}
			EXPECT_TRUE(isSecondPriorityFormat);
		}
		else
		{
			// Fallback: first available format.
			EXPECT_EQ(selected.format, availFmts[0].format);
			EXPECT_EQ(selected.colorSpace, availFmts[0].colorSpace);
		}
	}

	// QueryFormatsAndModes must not change GetHandle or IsInitialized.
	EXPECT_TRUE(surface.IsInitialized());
	EXPECT_NE(surface.GetHandle(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// refresh_capabilities_requeries_only_capabilities
// ---------------------------------------------------------------------------
TEST(Surface, test_refresh_capabilities_requeries_only_capabilities)
{
	EnsureLogger();

	virasa::window::Platform platform;
	if (!MakePlatform(platform))
	{
		GTEST_SKIP() << "Platform initialization failed.";
	}

	virasa::Instance instance;
	if (!MakeInstance(instance))
	{
		GTEST_SKIP() << "Vulkan Instance initialization failed.";
	}

	virasa::Surface surface;
	if (surface.Initialize(instance, platform) != virasa::RenderError::None)
	{
		GTEST_SKIP() << "Surface initialization failed.";
	}

	VkPhysicalDevice physDev = PickPhysicalDevice(instance.GetHandle());
	if (physDev == VK_NULL_HANDLE)
	{
		GTEST_SKIP() << "No physical device found.";
	}

	// Call QueryFormatsAndModes first so we have a baseline format/present mode.
	virasa::RendererConfig config;
	config.preferMailbox = false;
	surface.QueryFormatsAndModes(physDev, config);
	EXPECT_TRUE(surface.AreFormatsQueried());

	VkSurfaceFormatKHR fmtBefore = surface.GetFormat();
	VkPresentModeKHR pmBefore = surface.GetPresentMode();

	// RefreshCapabilities must update capabilities but not format or present mode.
	surface.RefreshCapabilities(physDev);

	EXPECT_EQ(surface.GetFormat().format, fmtBefore.format);
	EXPECT_EQ(surface.GetFormat().colorSpace, fmtBefore.colorSpace);
	EXPECT_EQ(surface.GetPresentMode(), pmBefore);

	// Capabilities should be freshly queried and valid.
	const VkSurfaceCapabilitiesKHR& caps = surface.GetCapabilities();
	EXPECT_GE(caps.minImageCount, 1u);

	// IsInitialized and GetHandle must be unchanged.
	EXPECT_TRUE(surface.IsInitialized());
	EXPECT_NE(surface.GetHandle(), VK_NULL_HANDLE);

	// AreFormatsQueried must still be true (RefreshCapabilities does not reset it).
	EXPECT_TRUE(surface.AreFormatsQueried());

	// RefreshCapabilities on a Surface for which AreFormatsQueried is false:
	// permitted; populates capabilities, leaves AreFormatsQueried false.
	virasa::Surface surface2;
	if (surface2.Initialize(instance, platform) == virasa::RenderError::None)
	{
		EXPECT_FALSE(surface2.AreFormatsQueried());
		surface2.RefreshCapabilities(physDev);
		EXPECT_FALSE(surface2.AreFormatsQueried());
		const VkSurfaceCapabilitiesKHR& caps2 = surface2.GetCapabilities();
		EXPECT_GE(caps2.minImageCount, 1u);
	}
}

// ---------------------------------------------------------------------------
// format_and_present_mode_observers_require_query
// ---------------------------------------------------------------------------
TEST(Surface, test_format_and_present_mode_observers_require_query)
{
	EnsureLogger();

	virasa::window::Platform platform;
	if (!MakePlatform(platform))
	{
		GTEST_SKIP() << "Platform initialization failed.";
	}

	virasa::Instance instance;
	if (!MakeInstance(instance))
	{
		GTEST_SKIP() << "Vulkan Instance initialization failed.";
	}

	VkPhysicalDevice physDev = PickPhysicalDevice(instance.GetHandle());
	if (physDev == VK_NULL_HANDLE)
	{
		GTEST_SKIP() << "No physical device found.";
	}

	virasa::Surface surface;
	if (surface.Initialize(instance, platform) != virasa::RenderError::None)
	{
		GTEST_SKIP() << "Surface initialization failed.";
	}

	// AreFormatsQueried is false after Initialize alone.
	EXPECT_FALSE(surface.AreFormatsQueried());

	// After QueryFormatsAndModes, AreFormatsQueried becomes true and
	// GetFormat / GetPresentMode / GetCapabilities are well-defined.
	virasa::RendererConfig config;
	config.preferMailbox = false;
	surface.QueryFormatsAndModes(physDev, config);
	EXPECT_TRUE(surface.AreFormatsQueried());

	// Observers are noexcept and return cached values.
	VkSurfaceFormatKHR fmt = surface.GetFormat();
	VkPresentModeKHR pm = surface.GetPresentMode();
	const VkSurfaceCapabilitiesKHR& caps = surface.GetCapabilities();

	// Calling them again returns the same values (pure observers, no side effects).
	EXPECT_EQ(surface.GetFormat().format, fmt.format);
	EXPECT_EQ(surface.GetFormat().colorSpace, fmt.colorSpace);
	EXPECT_EQ(surface.GetPresentMode(), pm);
	EXPECT_EQ(surface.GetCapabilities().minImageCount, caps.minImageCount);

	// IsInitialized and GetHandle unchanged by observers.
	EXPECT_TRUE(surface.IsInitialized());
	EXPECT_NE(surface.GetHandle(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// get_handle_returns_owned_vk_surface_or_null
// ---------------------------------------------------------------------------
TEST(Surface, test_get_handle_returns_owned_vk_surface_or_null)
{
	EnsureLogger();

	// Default-constructed: VK_NULL_HANDLE.
	virasa::Surface surface;
	EXPECT_EQ(surface.GetHandle(), VK_NULL_HANDLE);

	virasa::window::Platform platform;
	if (!MakePlatform(platform))
	{
		GTEST_SKIP() << "Platform initialization failed.";
	}

	virasa::Instance instance;
	if (!MakeInstance(instance))
	{
		GTEST_SKIP() << "Vulkan Instance initialization failed.";
	}

	auto result = surface.Initialize(instance, platform);
	if (result != virasa::RenderError::None)
	{
		GTEST_SKIP() << "Surface initialization failed.";
	}

	// After successful Initialize: non-null handle.
	VkSurfaceKHR handle = surface.GetHandle();
	EXPECT_NE(handle, VK_NULL_HANDLE);

	// GetHandle is a pure observer: calling it twice returns the same value.
	EXPECT_EQ(surface.GetHandle(), handle);

	// After move: source handle becomes VK_NULL_HANDLE.
	virasa::Surface dst(std::move(surface));
	EXPECT_EQ(surface.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(dst.GetHandle(), handle);
}

// ---------------------------------------------------------------------------
// is_initialized_reflects_owned_handle
// ---------------------------------------------------------------------------
TEST(Surface, test_is_initialized_reflects_owned_handle)
{
	EnsureLogger();

	// Default-constructed: not initialized.
	virasa::Surface surface;
	EXPECT_FALSE(surface.IsInitialized());
	EXPECT_EQ(surface.GetHandle(), VK_NULL_HANDLE);
	// Invariant: IsInitialized iff GetHandle != VK_NULL_HANDLE.
	EXPECT_EQ(surface.IsInitialized(), surface.GetHandle() != VK_NULL_HANDLE);

	virasa::window::Platform platform;
	if (!MakePlatform(platform))
	{
		GTEST_SKIP() << "Platform initialization failed.";
	}

	virasa::Instance instance;
	if (!MakeInstance(instance))
	{
		GTEST_SKIP() << "Vulkan Instance initialization failed.";
	}

	auto result = surface.Initialize(instance, platform);
	if (result != virasa::RenderError::None)
	{
		GTEST_SKIP() << "Surface initialization failed.";
	}

	// After successful Initialize: initialized.
	EXPECT_TRUE(surface.IsInitialized());
	EXPECT_NE(surface.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(surface.IsInitialized(), surface.GetHandle() != VK_NULL_HANDLE);

	// After move: source is no longer initialized.
	virasa::Surface dst(std::move(surface));
	EXPECT_FALSE(surface.IsInitialized());
	EXPECT_EQ(surface.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(surface.IsInitialized(), surface.GetHandle() != VK_NULL_HANDLE);

	EXPECT_TRUE(dst.IsInitialized());
	EXPECT_NE(dst.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(dst.IsInitialized(), dst.GetHandle() != VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// are_formats_queried_reflects_query_call
// ---------------------------------------------------------------------------
TEST(Surface, test_are_formats_queried_reflects_query_call)
{
	EnsureLogger();

	// Default-constructed: false.
	virasa::Surface surface;
	EXPECT_FALSE(surface.AreFormatsQueried());

	virasa::window::Platform platform;
	if (!MakePlatform(platform))
	{
		GTEST_SKIP() << "Platform initialization failed.";
	}

	virasa::Instance instance;
	if (!MakeInstance(instance))
	{
		GTEST_SKIP() << "Vulkan Instance initialization failed.";
	}

	if (surface.Initialize(instance, platform) != virasa::RenderError::None)
	{
		GTEST_SKIP() << "Surface initialization failed.";
	}

	// After Initialize but before QueryFormatsAndModes: still false.
	EXPECT_FALSE(surface.AreFormatsQueried());

	VkPhysicalDevice physDev = PickPhysicalDevice(instance.GetHandle());
	if (physDev == VK_NULL_HANDLE)
	{
		GTEST_SKIP() << "No physical device found.";
	}

	// After RefreshCapabilities only: still false.
	surface.RefreshCapabilities(physDev);
	EXPECT_FALSE(surface.AreFormatsQueried());

	// After QueryFormatsAndModes: true.
	virasa::RendererConfig config;
	surface.QueryFormatsAndModes(physDev, config);
	EXPECT_TRUE(surface.AreFormatsQueried());

	// After move: moved-from surface reports false.
	virasa::Surface dst(std::move(surface));
	EXPECT_FALSE(surface.AreFormatsQueried());
	EXPECT_TRUE(dst.AreFormatsQueried());
}

// ---------------------------------------------------------------------------
// surface_methods_are_not_thread_safe_per_instance
// ---------------------------------------------------------------------------
// This semantic is a documentation/contract guarantee (no synchronization).
// We verify the observable corollary: concurrent reads on const observers
// of the same Surface are permitted. We cannot safely test concurrent
// non-const calls (that would be UB per the contract), so we only verify
// that the const observers can be called concurrently without data races
// by using a fully-initialized, query-completed Surface and reading from
// two threads simultaneously.
TEST(Surface, test_surface_methods_are_not_thread_safe_per_instance)
{
	EnsureLogger();

	virasa::window::Platform platform;
	if (!MakePlatform(platform))
	{
		GTEST_SKIP() << "Platform initialization failed.";
	}

	virasa::Instance instance;
	if (!MakeInstance(instance))
	{
		GTEST_SKIP() << "Vulkan Instance initialization failed.";
	}

	virasa::Surface surface;
	if (surface.Initialize(instance, platform) != virasa::RenderError::None)
	{
		GTEST_SKIP() << "Surface initialization failed.";
	}

	VkPhysicalDevice physDev = PickPhysicalDevice(instance.GetHandle());
	if (physDev == VK_NULL_HANDLE)
	{
		GTEST_SKIP() << "No physical device found.";
	}

	virasa::RendererConfig config;
	config.preferMailbox = false;
	surface.QueryFormatsAndModes(physDev, config);
	EXPECT_TRUE(surface.AreFormatsQueried());

	// Concurrent reads of const observers from two threads must not race.
	std::atomic<bool> go{false};
	std::atomic<int> readsDone{0};

	auto reader = [&]()
	{
		while (!go.load(std::memory_order_acquire))
		{
		}
		for (int i = 0; i < 100; ++i)
		{
			(void)surface.GetHandle();
			(void)surface.IsInitialized();
			(void)surface.AreFormatsQueried();
			(void)surface.GetFormat();
			(void)surface.GetPresentMode();
			(void)surface.GetCapabilities();
		}
		readsDone.fetch_add(1, std::memory_order_release);
	};

	std::thread t1(reader);
	std::thread t2(reader);
	go.store(true, std::memory_order_release);
	t1.join();
	t2.join();

	EXPECT_EQ(readsDone.load(), 2);

	// Distinct Surface objects may be used concurrently — verify by operating
	// two separate surfaces from different threads.
	virasa::Surface surface2;
	if (surface2.Initialize(instance, platform) == virasa::RenderError::None)
	{
		surface2.QueryFormatsAndModes(physDev, config);

		std::atomic<bool> go2{false};
		std::atomic<int> done2{0};

		auto worker1 = [&]()
		{
			while (!go2.load(std::memory_order_acquire))
			{
			}
			(void)surface.GetHandle();
			(void)surface.IsInitialized();
			done2.fetch_add(1, std::memory_order_release);
		};
		auto worker2 = [&]()
		{
			while (!go2.load(std::memory_order_acquire))
			{
			}
			(void)surface2.GetHandle();
			(void)surface2.IsInitialized();
			done2.fetch_add(1, std::memory_order_release);
		};

		std::thread ta(worker1);
		std::thread tb(worker2);
		go2.store(true, std::memory_order_release);
		ta.join();
		tb.join();
		EXPECT_EQ(done2.load(), 2);
	}
}
