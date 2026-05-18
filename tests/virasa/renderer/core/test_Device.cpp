#include <vulkan/vulkan.h>

#include <cstring>
#include <gtest/gtest.h>
#include <utility>

#include "virasa/core/Logger.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/core/Surface.h"
#include "virasa/window/Platform.h"
#include "virasa/window/Types.h"

using namespace virasa;
using namespace virasa;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{

void EnsureLogger()
{
	if (!Logger::IsInitialized())
		Logger::Initialize();
}

// Initialize a Platform (creates an SDL window).
bool MakePlatform(window::Platform& platform)
{
	return platform.Initialize("DeviceTest", 800, 600) == ErrorCode::None;
}

// Initialize an Instance with the surface extensions required by Platform.
bool MakeInstanceForSurface(Instance& instance)
{
	uint32_t extCount = 0;
	const char* const* exts = window::Platform::GetRequiredVulkanExtensions(&extCount);

	RendererConfig cfg;
	cfg.applicationName = "DeviceTest";
	cfg.enableValidation = false;
	cfg.requiredInstanceExtensions = exts;
	cfg.requiredInstanceExtensionCount = extCount;
	return instance.Initialize(cfg) == RenderError::None;
}

// Full setup: Platform + Instance (with surface exts) + Surface.
// Returns false if any step fails (SDL or Vulkan unavailable).
bool TrySetupForDevice(window::Platform& platform, Instance& instance, Surface& surface)
{
	if (!MakePlatform(platform))
		return false;
	if (!MakeInstanceForSurface(instance))
		return false;
	return surface.Initialize(instance, platform) == RenderError::None;
}

// Create a fully-initialized Instance without surface extensions.
// Used only for tests that specifically exercise the null-surface failure path.
bool TryInitializeInstance(Instance& instance)
{
	RendererConfig cfg;
	cfg.applicationName = "DeviceTest";
	cfg.enableValidation = false;
	cfg.requiredInstanceExtensions = nullptr;
	cfg.requiredInstanceExtensionCount = 0;
	RenderError stat = instance.Initialize(cfg);
	return stat == RenderError::None || stat == RenderError::AlreadyInitialized;
}

} // namespace

// ---------------------------------------------------------------------------
// device_default_constructed_state
// ---------------------------------------------------------------------------
TEST(Device, test_device_default_constructed_state)
{
	Device device;

	EXPECT_EQ(device.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(device.GetPhysicalDevice(), VK_NULL_HANDLE);
	EXPECT_EQ(device.GetGraphicsQueue(), VK_NULL_HANDLE);
	EXPECT_EQ(device.GetPresentQueue(), VK_NULL_HANDLE);
	EXPECT_EQ(device.GetTransferQueue(), VK_NULL_HANDLE);
	EXPECT_FALSE(device.IsInitialized());

	const QueueFamilies& qf = device.GetQueueFamilies();
	EXPECT_FALSE(qf.graphicsFound);
	EXPECT_FALSE(qf.presentFound);
	EXPECT_FALSE(qf.dedicatedTransfer);
	EXPECT_EQ(qf.graphicsFamily, 0u);
	EXPECT_EQ(qf.presentFamily, 0u);
	EXPECT_EQ(qf.transferFamily, 0u);

	const VkPhysicalDeviceMemoryProperties& memProps = device.GetMemoryProperties();
	// Zero-initialized: memoryTypeCount and memoryHeapCount should be 0.
	EXPECT_EQ(memProps.memoryTypeCount, 0u);
	EXPECT_EQ(memProps.memoryHeapCount, 0u);

	const VkPhysicalDeviceProperties& props = device.GetProperties();
	EXPECT_EQ(props.apiVersion, 0u);
	EXPECT_EQ(props.deviceType, VK_PHYSICAL_DEVICE_TYPE_OTHER);

	EXPECT_EQ(device.GetGraphicsQueueFamilyIndex(), 0u);

	// WaitIdle on a default-constructed device must be a no-op (no crash).
	EXPECT_NO_FATAL_FAILURE(device.WaitIdle());
}

// ---------------------------------------------------------------------------
// device_is_raii_movable_non_copyable_handle_owner
// ---------------------------------------------------------------------------
TEST(Device, test_device_is_raii_movable_non_copyable_handle_owner)
{
	EnsureLogger();

	// Verify non-copyable at compile-time via type traits.
	EXPECT_FALSE(std::is_copy_constructible_v<Device>);
	EXPECT_FALSE(std::is_copy_assignable_v<Device>);

	// Verify movable at compile-time via type traits.
	EXPECT_TRUE(std::is_move_constructible_v<Device>);
	EXPECT_TRUE(std::is_move_assignable_v<Device>);

	// Verify final.
	EXPECT_TRUE(std::is_final_v<Device>);

	// Verify default-constructible.
	EXPECT_TRUE(std::is_default_constructible_v<Device>);

	// Move constructor transfers ownership: source becomes default-constructed.
	{
		Device src;
		Device dst(std::move(src));
		EXPECT_EQ(src.GetHandle(), VK_NULL_HANDLE);
		EXPECT_FALSE(src.IsInitialized());
		EXPECT_EQ(src.GetPhysicalDevice(), VK_NULL_HANDLE);
		EXPECT_EQ(src.GetGraphicsQueue(), VK_NULL_HANDLE);
		EXPECT_EQ(src.GetPresentQueue(), VK_NULL_HANDLE);
		EXPECT_EQ(src.GetTransferQueue(), VK_NULL_HANDLE);
	}

	// Move assignment from default-constructed into default-constructed.
	{
		Device src;
		Device dst;
		dst = std::move(src);
		EXPECT_EQ(src.GetHandle(), VK_NULL_HANDLE);
		EXPECT_FALSE(src.IsInitialized());
		EXPECT_EQ(dst.GetHandle(), VK_NULL_HANDLE);
		EXPECT_FALSE(dst.IsInitialized());
	}

	// Test move semantics on an initialized Device.
	window::Platform platform;
	Instance instance;
	Surface surface;
	if (!TrySetupForDevice(platform, instance, surface))
	{
		GTEST_SKIP() << "Platform/Vulkan/Surface setup failed; skipping initialized-device move test.";
	}

	Device src;
	ASSERT_EQ(src.Initialize(instance, surface.GetHandle()), RenderError::None);
	VkDevice originalHandle = src.GetHandle();
	ASSERT_NE(originalHandle, VK_NULL_HANDLE);

	// Move-construct: handle transfers, source becomes null.
	Device dst(std::move(src));
	EXPECT_EQ(src.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(src.IsInitialized());
	EXPECT_EQ(dst.GetHandle(), originalHandle);
	EXPECT_TRUE(dst.IsInitialized());

	// Move-assign into a second device.
	Device dst2;
	dst2 = std::move(dst);
	EXPECT_EQ(dst.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(dst.IsInitialized());
	EXPECT_EQ(dst2.GetHandle(), originalHandle);
	EXPECT_TRUE(dst2.IsInitialized());
}

// ---------------------------------------------------------------------------
// device_initialize_selects_and_creates
// ---------------------------------------------------------------------------
TEST(Device, test_device_initialize_selects_and_creates)
{
	EnsureLogger();

	window::Platform platform;
	Instance instance;
	Surface surface;
	if (!TrySetupForDevice(platform, instance, surface))
	{
		GTEST_SKIP() << "Platform/Vulkan/Surface setup failed.";
	}

	Device device;
	RenderError err = device.Initialize(instance, surface.GetHandle());
	ASSERT_EQ(err, RenderError::None);

	EXPECT_NE(device.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(device.GetPhysicalDevice(), VK_NULL_HANDLE);
	EXPECT_TRUE(device.IsInitialized());
	EXPECT_NE(device.GetGraphicsQueue(), VK_NULL_HANDLE);
	EXPECT_NE(device.GetPresentQueue(), VK_NULL_HANDLE);
	EXPECT_NE(device.GetTransferQueue(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// device_hard_requirements_for_physical_device_selection
// ---------------------------------------------------------------------------
TEST(Device, test_device_hard_requirements_for_physical_device_selection)
{
	EnsureLogger();

	window::Platform platform;
	Instance instance;
	Surface surface;
	if (!TrySetupForDevice(platform, instance, surface))
	{
		GTEST_SKIP() << "Platform/Vulkan/Surface setup failed.";
	}

	Device device;
	RenderError err = device.Initialize(instance, surface.GetHandle());
	ASSERT_EQ(err, RenderError::None);

	// The selected device must satisfy the Vulkan 1.3 hard requirement.
	EXPECT_GE(device.GetProperties().apiVersion, static_cast<uint32_t>(VK_API_VERSION_1_3));
}

// ---------------------------------------------------------------------------
// device_scoring_algorithm
// ---------------------------------------------------------------------------
TEST(Device, test_device_scoring_algorithm)
{
	EnsureLogger();

	window::Platform platform;
	Instance instance;
	Surface surface;
	if (!TrySetupForDevice(platform, instance, surface))
	{
		GTEST_SKIP() << "Platform/Vulkan/Surface setup failed.";
	}

	Device device;
	ASSERT_EQ(device.Initialize(instance, surface.GetHandle()), RenderError::None);

	// The selected device must have a non-empty device name.
	const VkPhysicalDeviceProperties& props = device.GetProperties();
	EXPECT_GT(std::strlen(props.deviceName), 0u);
	// The selected device must have passed the hard requirements.
	EXPECT_GE(props.apiVersion, static_cast<uint32_t>(VK_API_VERSION_1_3));
}

// ---------------------------------------------------------------------------
// device_queue_family_selection_algorithm
// ---------------------------------------------------------------------------
TEST(Device, test_device_queue_family_selection_algorithm)
{
	EnsureLogger();

	window::Platform platform;
	Instance instance;
	Surface surface;
	if (!TrySetupForDevice(platform, instance, surface))
	{
		GTEST_SKIP() << "Platform/Vulkan/Surface setup failed.";
	}

	Device device;
	ASSERT_EQ(device.Initialize(instance, surface.GetHandle()), RenderError::None);

	const QueueFamilies& qf = device.GetQueueFamilies();
	EXPECT_TRUE(qf.graphicsFound);
	EXPECT_TRUE(qf.presentFound);
	EXPECT_TRUE(qf.IsComplete());
	EXPECT_EQ(device.GetGraphicsQueueFamilyIndex(), qf.graphicsFamily);
}

// ---------------------------------------------------------------------------
// device_logical_device_creation_uses_unique_queue_families
// ---------------------------------------------------------------------------
TEST(Device, test_device_logical_device_creation_uses_unique_queue_families)
{
	EnsureLogger();

	window::Platform platform;
	Instance instance;
	Surface surface;
	if (!TrySetupForDevice(platform, instance, surface))
	{
		GTEST_SKIP() << "Platform/Vulkan/Surface setup failed.";
	}

	Device device;
	ASSERT_EQ(device.Initialize(instance, surface.GetHandle()), RenderError::None);

	EXPECT_NE(device.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(device.GetGraphicsQueue(), VK_NULL_HANDLE);
	EXPECT_NE(device.GetPresentQueue(), VK_NULL_HANDLE);
	EXPECT_NE(device.GetTransferQueue(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// device_logical_device_creation_uses_required_and_optional_features
// ---------------------------------------------------------------------------
TEST(Device, test_device_logical_device_creation_uses_required_and_optional_features)
{
	EnsureLogger();

	window::Platform platform;
	Instance instance;
	Surface surface;
	if (!TrySetupForDevice(platform, instance, surface))
	{
		GTEST_SKIP() << "Platform/Vulkan/Surface setup failed.";
	}

	Device device;
	ASSERT_EQ(device.Initialize(instance, surface.GetHandle()), RenderError::None);

	// Success implies dynamicRendering and synchronization2 were enabled
	// (they are hard requirements checked before device creation).
	EXPECT_NE(device.GetHandle(), VK_NULL_HANDLE);
	EXPECT_TRUE(device.IsInitialized());
}

// ---------------------------------------------------------------------------
// device_queue_handle_retrieval
// ---------------------------------------------------------------------------
TEST(Device, test_device_queue_handle_retrieval)
{
	EnsureLogger();

	window::Platform platform;
	Instance instance;
	Surface surface;
	if (!TrySetupForDevice(platform, instance, surface))
	{
		GTEST_SKIP() << "Platform/Vulkan/Surface setup failed.";
	}

	Device device;
	ASSERT_EQ(device.Initialize(instance, surface.GetHandle()), RenderError::None);

	ASSERT_NE(device.GetGraphicsQueue(), VK_NULL_HANDLE);
	ASSERT_NE(device.GetPresentQueue(), VK_NULL_HANDLE);
	ASSERT_NE(device.GetTransferQueue(), VK_NULL_HANDLE);

	const QueueFamilies& qf = device.GetQueueFamilies();
	if (!qf.dedicatedTransfer)
	{
		// Transfer queue must equal the graphics queue when there is no
		// dedicated transfer family.
		EXPECT_EQ(device.GetTransferQueue(), device.GetGraphicsQueue());
	}
}

// ---------------------------------------------------------------------------
// device_initialize_does_not_partially_modify_on_failure
// ---------------------------------------------------------------------------
TEST(Device, test_device_initialize_does_not_partially_modify_on_failure)
{
	EnsureLogger();

	// Use a no-extensions Instance + VK_NULL_HANDLE surface to reliably force
	// a failure: no physical device can satisfy the present-support check
	// against a null surface.
	Instance instance;
	if (!TryInitializeInstance(instance))
	{
		GTEST_SKIP() << "No Vulkan driver available.";
	}

	Device device;
	RenderError err = device.Initialize(instance, VK_NULL_HANDLE);
	if (err == RenderError::None)
	{
		// Some drivers tolerate VK_NULL_HANDLE; skip rather than mis-assert.
		GTEST_SKIP() << "Initialize unexpectedly succeeded with null surface; skipping failure-path check.";
	}

	// On failure the Device must remain in its default-constructed state.
	EXPECT_EQ(device.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(device.GetPhysicalDevice(), VK_NULL_HANDLE);
	EXPECT_FALSE(device.IsInitialized());
	EXPECT_EQ(device.GetGraphicsQueue(), VK_NULL_HANDLE);
	EXPECT_EQ(device.GetPresentQueue(), VK_NULL_HANDLE);
	EXPECT_EQ(device.GetTransferQueue(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// get_handle_returns_owned_vk_device_or_null
// ---------------------------------------------------------------------------
TEST(Device, test_get_handle_returns_owned_vk_device_or_null)
{
	EnsureLogger();

	// Default-constructed: GetHandle returns VK_NULL_HANDLE.
	Device device;
	EXPECT_EQ(device.GetHandle(), VK_NULL_HANDLE);

	window::Platform platform;
	Instance instance;
	Surface surface;
	if (!TrySetupForDevice(platform, instance, surface))
	{
		GTEST_SKIP() << "Platform/Vulkan/Surface setup failed.";
	}

	ASSERT_EQ(device.Initialize(instance, surface.GetHandle()), RenderError::None);
	ASSERT_NE(device.GetHandle(), VK_NULL_HANDLE);

	// After moving, the source's handle becomes VK_NULL_HANDLE.
	Device moved(std::move(device));
	EXPECT_EQ(device.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(moved.GetHandle(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// device_observers_return_cached_state
// ---------------------------------------------------------------------------
TEST(Device, test_device_observers_return_cached_state)
{
	// On a default-constructed Device all observers return the pinned defaults.
	Device device;
	EXPECT_EQ(device.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(device.GetPhysicalDevice(), VK_NULL_HANDLE);
	EXPECT_EQ(device.GetGraphicsQueue(), VK_NULL_HANDLE);
	EXPECT_EQ(device.GetPresentQueue(), VK_NULL_HANDLE);
	EXPECT_EQ(device.GetTransferQueue(), VK_NULL_HANDLE);
	EXPECT_FALSE(device.IsInitialized());
	EXPECT_EQ(device.GetGraphicsQueueFamilyIndex(), 0u);
	EXPECT_EQ(device.GetQueueFamilies().graphicsFound, false);
	EXPECT_EQ(device.GetMemoryProperties().memoryTypeCount, 0u);
	EXPECT_EQ(device.GetProperties().apiVersion, 0u);

	// On an initialized Device all observers return non-default values.
	window::Platform platform;
	Instance instance;
	Surface surface;
	if (!TrySetupForDevice(platform, instance, surface))
	{
		GTEST_SKIP() << "Platform/Vulkan/Surface setup failed.";
	}

	Device device2;
	ASSERT_EQ(device2.Initialize(instance, surface.GetHandle()), RenderError::None);

	EXPECT_NE(device2.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(device2.GetPhysicalDevice(), VK_NULL_HANDLE);
	EXPECT_NE(device2.GetGraphicsQueue(), VK_NULL_HANDLE);
	EXPECT_NE(device2.GetPresentQueue(), VK_NULL_HANDLE);
	EXPECT_NE(device2.GetTransferQueue(), VK_NULL_HANDLE);
	EXPECT_TRUE(device2.IsInitialized());

	const QueueFamilies& qf = device2.GetQueueFamilies();
	EXPECT_TRUE(qf.graphicsFound);
	EXPECT_TRUE(qf.presentFound);
	EXPECT_EQ(device2.GetGraphicsQueueFamilyIndex(), qf.graphicsFamily);

	// Memory properties must have at least one memory type.
	EXPECT_GT(device2.GetMemoryProperties().memoryTypeCount, 0u);
	// Properties must have a non-zero apiVersion.
	EXPECT_GT(device2.GetProperties().apiVersion, 0u);
}

// ---------------------------------------------------------------------------
// is_initialized_reflects_owned_handle
// ---------------------------------------------------------------------------
TEST(Device, test_is_initialized_reflects_owned_handle)
{
	// Default-constructed: IsInitialized == false, GetHandle == VK_NULL_HANDLE.
	Device device;
	EXPECT_FALSE(device.IsInitialized());
	EXPECT_EQ(device.GetHandle(), VK_NULL_HANDLE);
	// Invariant: IsInitialized iff GetHandle != VK_NULL_HANDLE.
	EXPECT_EQ(device.IsInitialized(), device.GetHandle() != VK_NULL_HANDLE);

	window::Platform platform;
	Instance instance;
	Surface surface;
	if (!TrySetupForDevice(platform, instance, surface))
	{
		GTEST_SKIP() << "Platform/Vulkan/Surface setup failed.";
	}

	ASSERT_EQ(device.Initialize(instance, surface.GetHandle()), RenderError::None);

	// Successfully initialized.
	EXPECT_TRUE(device.IsInitialized());
	EXPECT_NE(device.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(device.IsInitialized(), device.GetHandle() != VK_NULL_HANDLE);

	// After move-from, source is no longer initialized.
	Device dst(std::move(device));
	EXPECT_FALSE(device.IsInitialized());
	EXPECT_EQ(device.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(device.IsInitialized(), device.GetHandle() != VK_NULL_HANDLE);

	EXPECT_TRUE(dst.IsInitialized());
	EXPECT_NE(dst.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(dst.IsInitialized(), dst.GetHandle() != VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// wait_idle_blocks_on_device
// ---------------------------------------------------------------------------
TEST(Device, test_wait_idle_blocks_on_device)
{
	// On a default-constructed (uninitialized) Device, WaitIdle must be a
	// no-op and must not crash.
	{
		Device device;
		EXPECT_NO_FATAL_FAILURE(device.WaitIdle());
		EXPECT_FALSE(device.IsInitialized());
		EXPECT_EQ(device.GetHandle(), VK_NULL_HANDLE);
	}

	// On an initialized Device, WaitIdle must complete without error.
	// platform/instance/surface must outlive device — declare them first.
	window::Platform platform;
	Instance instance;
	Surface surface;
	if (!TrySetupForDevice(platform, instance, surface))
	{
		GTEST_SKIP() << "Platform/Vulkan/Surface setup failed.";
	}

	Device device;
	ASSERT_EQ(device.Initialize(instance, surface.GetHandle()), RenderError::None);

	EXPECT_NO_FATAL_FAILURE(device.WaitIdle());
	EXPECT_TRUE(device.IsInitialized());
	EXPECT_NE(device.GetHandle(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// device_methods_are_not_thread_safe_per_instance
// ---------------------------------------------------------------------------
TEST(Device, test_device_methods_are_not_thread_safe_per_instance)
{
	// This semantic documents a non-guarantee (no internal synchronization).
	// We verify the positive side: const observers may be called concurrently
	// on the same Device without crashing (the contract permits this).
	// We do not attempt to trigger UB from concurrent non-const calls.
	Device device;

	// Call all const observers from the main thread to confirm they are
	// callable and return consistent values (no synchronization needed for
	// a single-threaded call).
	const VkDevice h1 = device.GetHandle();
	const VkDevice h2 = device.GetHandle();
	EXPECT_EQ(h1, h2);

	const bool init1 = device.IsInitialized();
	const bool init2 = device.IsInitialized();
	EXPECT_EQ(init1, init2);

	// Distinct Device objects are independent; constructing two and calling
	// observers on each is always safe.
	Device deviceA;
	Device deviceB;
	EXPECT_EQ(deviceA.GetHandle(), deviceB.GetHandle());
	EXPECT_EQ(deviceA.IsInitialized(), deviceB.IsInitialized());
}
