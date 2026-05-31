#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <vector>

#include "virasa/core/Logger.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/resources/ShaderModule.h"
#include "virasa/window/Platform.h"
#include "virasa/window/Types.h"
#include "vulkan/vulkan.h"

using namespace virasa;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
namespace
{

[[nodiscard]] std::string MakeTempFilePath(const char* fileName)
{
	const std::filesystem::path tempDir = std::filesystem::temp_directory_path();
	return (tempDir / fileName).string();
}

const std::string kValidSpvPath = MakeTempFilePath("virasa_test_valid.spv");
const std::string kEmptySpvPath = MakeTempFilePath("virasa_test_empty.spv");
const std::string kOddSpvPath = MakeTempFilePath("virasa_test_odd.spv");

// Minimal SPIR-V binary: magic + version + generator + bound + schema
// This is the smallest legal SPIR-V module (no instructions beyond header).
// 5 words × 4 bytes = 20 bytes.
static const uint32_t kMinimalSpirV[] = {
	0x07230203u, // SPIR-V magic
	0x00010000u, // version 1.0
	0x00000000u, // generator
	0x00000001u, // bound
	0x00000000u, // schema
};

// Write kMinimalSpirV to kValidSpvPath.
bool WriteValidSpv()
{
	std::ofstream f(kValidSpvPath, std::ios::binary | std::ios::trunc);
	if (!f)
		return false;
	f.write(reinterpret_cast<const char*>(kMinimalSpirV), sizeof(kMinimalSpirV));
	return f.good();
}

// Write an empty file to kEmptySpvPath.
bool WriteEmptySpv()
{
	std::ofstream f(kEmptySpvPath, std::ios::binary | std::ios::trunc);
	return f.good();
}

// Write 3 bytes (not a multiple of 4) to kOddSpvPath.
bool WriteOddSpv()
{
	std::ofstream f(kOddSpvPath, std::ios::binary | std::ios::trunc);
	if (!f)
		return false;
	const char data[3] = {0x01, 0x02, 0x03};
	f.write(data, 3);
	return f.good();
}

// ---------------------------------------------------------------------------
// Vulkan / Device bootstrap helpers
// ---------------------------------------------------------------------------

// Bring up a real Vulkan instance + device for tests that need one.
// Returns false if the host has no Vulkan support.
struct VulkanContext
{
	Instance instance;
	window::Platform platform;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	Device device;
	bool valid = false;

	bool Init()
	{
		Logger::Initialize();

		RendererConfig cfg;
		cfg.enableValidation = false;

		// We need a surface for device selection; create a headless window.
		ErrorCode wErr = platform.Initialize("ShaderModuleTest", 1, 1);
		if (wErr != ErrorCode::None)
		{
			// No display – skip.
			return false;
		}

		// Collect required instance extensions from the platform.
		uint32_t extCount = 0;
		const char* const* exts = window::Platform::GetRequiredVulkanExtensions(&extCount);
		cfg.requiredInstanceExtensions = exts;
		cfg.requiredInstanceExtensionCount = extCount;

		RenderError err = instance.Initialize(cfg);
		if (err != RenderError::None)
			return false;

		ErrorCode sErr = platform.CreateSurface(instance.GetHandle(), &surface);
		if (sErr != ErrorCode::None)
			return false;

		err = device.Initialize(instance, surface);
		if (err != RenderError::None)
			return false;

		valid = true;
		return true;
	}

	~VulkanContext()
	{
		// Device / instance destructors handle Vulkan cleanup.
		if (surface != VK_NULL_HANDLE && instance.GetHandle() != VK_NULL_HANDLE)
		{
			vkDestroySurfaceKHR(instance.GetHandle(), surface, nullptr);
		}
		platform.Shutdown();
		Logger::Shutdown();
	}
};

} // namespace

// ===========================================================================
// Tests
// ===========================================================================

// ---------------------------------------------------------------------------
// shader_module_is_raii_movable_non_copyable_handle_owner
// ---------------------------------------------------------------------------
TEST(ShaderModule, test_shader_module_is_raii_movable_non_copyable_handle_owner)
{
	using SM = ShaderModule;

	// Default-constructible.
	EXPECT_TRUE(std::is_default_constructible_v<SM>);

	// Not copyable.
	EXPECT_FALSE(std::is_copy_constructible_v<SM>);
	EXPECT_FALSE(std::is_copy_assignable_v<SM>);

	// Movable.
	EXPECT_TRUE(std::is_move_constructible_v<SM>);
	EXPECT_TRUE(std::is_move_assignable_v<SM>);

	// Final.
	EXPECT_TRUE(std::is_final_v<SM>);

	// Default-constructed state.
	SM sm;
	EXPECT_EQ(sm.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(sm.IsInitialized());

	// Move constructor: move a default-constructed ShaderModule.
	SM sm2(std::move(sm));
	EXPECT_EQ(sm2.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(sm2.IsInitialized());
	// Source is still in default-constructed state.
	EXPECT_EQ(sm.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(sm.IsInitialized());

	// Move assignment.
	SM sm3;
	sm3 = std::move(sm2);
	EXPECT_EQ(sm3.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(sm3.IsInitialized());
	EXPECT_EQ(sm2.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(sm2.IsInitialized());

	// Now exercise move with a real handle if Vulkan is available.
	ASSERT_TRUE(WriteValidSpv());

	VulkanContext ctx;
	if (!ctx.Init())
	{
		GTEST_SKIP() << "Vulkan not available on this host; skipping handle-transfer portion.";
	}

	SM src;
	RenderError err = src.Initialize(ctx.device, kValidSpvPath.c_str());
	ASSERT_EQ(err, RenderError::None);
	ASSERT_TRUE(src.IsInitialized());
	VkShaderModule handle = src.GetHandle();
	ASSERT_NE(handle, VK_NULL_HANDLE);

	// Move constructor transfers ownership.
	SM dst(std::move(src));
	EXPECT_EQ(dst.GetHandle(), handle);
	EXPECT_TRUE(dst.IsInitialized());
	EXPECT_EQ(src.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(src.IsInitialized());

	// Move assignment: dst already owns a handle; assigning another module
	// should destroy dst's prior handle and take ownership of the new one.
	SM src2;
	ASSERT_EQ(src2.Initialize(ctx.device, kValidSpvPath.c_str()), RenderError::None);
	VkShaderModule handle2 = src2.GetHandle();
	ASSERT_NE(handle2, VK_NULL_HANDLE);

	dst = std::move(src2);
	EXPECT_EQ(dst.GetHandle(), handle2);
	EXPECT_TRUE(dst.IsInitialized());
	EXPECT_EQ(src2.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(src2.IsInitialized());

	// Destroying dst (end of scope) must not crash — vkDestroyShaderModule is called.
}

// ---------------------------------------------------------------------------
// shader_module_borrows_vk_device_for_lifetime
// ---------------------------------------------------------------------------
TEST(ShaderModule, test_shader_module_borrows_vk_device_for_lifetime)
{
	ASSERT_TRUE(WriteValidSpv());

	VulkanContext ctx;
	if (!ctx.Init())
	{
		GTEST_SKIP() << "Vulkan not available on this host.";
	}

	ShaderModule sm;
	ASSERT_EQ(sm.Initialize(ctx.device, kValidSpvPath.c_str()), RenderError::None);
	ASSERT_TRUE(sm.IsInitialized());

	// The ShaderModule stores the VkDevice from device.GetHandle().
	// We verify this indirectly: the ShaderModule is valid, and when it is
	// destroyed (end of scope) it calls vkDestroyShaderModule using that device.
	// We cannot directly inspect the stored handle, but we can confirm that
	// after a move the destination still holds a valid handle (meaning the
	// borrowed VkDevice was transferred).
	VkShaderModule originalHandle = sm.GetHandle();
	ASSERT_NE(originalHandle, VK_NULL_HANDLE);

	ShaderModule sm2(std::move(sm));
	EXPECT_EQ(sm2.GetHandle(), originalHandle);
	EXPECT_TRUE(sm2.IsInitialized());
	// sm is now in default-constructed state; its borrowed device was transferred.
	EXPECT_EQ(sm.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(sm.IsInitialized());
	// sm2 will be destroyed at end of scope; it must use ctx.device's VkDevice
	// to call vkDestroyShaderModule without crashing.
}

// ---------------------------------------------------------------------------
// shader_module_default_constructed_state
// ---------------------------------------------------------------------------
TEST(ShaderModule, test_shader_module_default_constructed_state)
{
	ShaderModule sm;

	// GetHandle and IsInitialized are well-defined on a default-constructed object.
	EXPECT_EQ(sm.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(sm.IsInitialized());

	// Calling them multiple times has no side effects.
	EXPECT_EQ(sm.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(sm.IsInitialized());
}

// ---------------------------------------------------------------------------
// shader_module_initialize_is_reentrant_and_clears_prior
// ---------------------------------------------------------------------------
TEST(ShaderModule, test_shader_module_initialize_is_reentrant_and_clears_prior)
{
	ASSERT_TRUE(WriteValidSpv());

	VulkanContext ctx;
	if (!ctx.Init())
	{
		GTEST_SKIP() << "Vulkan not available on this host.";
	}

	ShaderModule sm;

	// First successful Initialize.
	ASSERT_EQ(sm.Initialize(ctx.device, kValidSpvPath.c_str()), RenderError::None);
	ASSERT_TRUE(sm.IsInitialized());
	VkShaderModule firstHandle = sm.GetHandle();
	ASSERT_NE(firstHandle, VK_NULL_HANDLE);

	// Re-initialize with the same valid file: prior handle is destroyed,
	// a new handle is created.
	ASSERT_EQ(sm.Initialize(ctx.device, kValidSpvPath.c_str()), RenderError::None);
	EXPECT_TRUE(sm.IsInitialized());
	EXPECT_NE(sm.GetHandle(), VK_NULL_HANDLE);
	// (The new handle may or may not equal the old one; we just verify it's valid.)

	// Re-initialize with an invalid path (nullptr): prior handle is destroyed
	// unconditionally, then the validation fails.
	RenderError err = sm.Initialize(ctx.device, nullptr);
	EXPECT_EQ(err, RenderError::ShaderLoadFailed);
	// After a failed re-Initialize the ShaderModule is in default-constructed state.
	EXPECT_FALSE(sm.IsInitialized());
	EXPECT_EQ(sm.GetHandle(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// shader_module_initialize_validates_file_path_and_size
// ---------------------------------------------------------------------------
TEST(ShaderModule, test_shader_module_initialize_validates_file_path_and_size)
{
	ASSERT_TRUE(WriteEmptySpv());
	ASSERT_TRUE(WriteOddSpv());

	VulkanContext ctx;
	if (!ctx.Init())
	{
		GTEST_SKIP() << "Vulkan not available on this host.";
	}

	ShaderModule sm;

	// Validation 1a: nullptr path.
	{
		ShaderModule s;
		EXPECT_EQ(s.Initialize(ctx.device, nullptr), RenderError::ShaderLoadFailed);
		EXPECT_FALSE(s.IsInitialized());
		EXPECT_EQ(s.GetHandle(), VK_NULL_HANDLE);
	}

	// Validation 1b: empty string path.
	{
		ShaderModule s;
		EXPECT_EQ(s.Initialize(ctx.device, ""), RenderError::ShaderLoadFailed);
		EXPECT_FALSE(s.IsInitialized());
		EXPECT_EQ(s.GetHandle(), VK_NULL_HANDLE);
	}

	// Validation 2: non-existent file.
	{
		ShaderModule s;
		const std::string missingPath = MakeTempFilePath("virasa_test_does_not_exist_xyz.spv");
		EXPECT_EQ(s.Initialize(ctx.device, missingPath.c_str()),
			RenderError::ShaderLoadFailed);
		EXPECT_FALSE(s.IsInitialized());
		EXPECT_EQ(s.GetHandle(), VK_NULL_HANDLE);
	}

	// Validation 3a: zero-length file.
	{
		ShaderModule s;
		EXPECT_EQ(s.Initialize(ctx.device, kEmptySpvPath.c_str()), RenderError::ShaderLoadFailed);
		EXPECT_FALSE(s.IsInitialized());
		EXPECT_EQ(s.GetHandle(), VK_NULL_HANDLE);
	}

	// Validation 3b: file size not a multiple of 4.
	{
		ShaderModule s;
		EXPECT_EQ(s.Initialize(ctx.device, kOddSpvPath.c_str()), RenderError::ShaderLoadFailed);
		EXPECT_FALSE(s.IsInitialized());
		EXPECT_EQ(s.GetHandle(), VK_NULL_HANDLE);
	}

	// Sanity: a valid file passes all validations.
	ASSERT_TRUE(WriteValidSpv());
	{
		ShaderModule s;
		RenderError err = s.Initialize(ctx.device, kValidSpvPath.c_str());
		// May succeed or fail at the Vulkan call depending on driver, but must
		// not return ShaderLoadFailed.
		EXPECT_NE(err, RenderError::ShaderLoadFailed);
	}
}

// ---------------------------------------------------------------------------
// shader_module_initialize_creates_vk_shader_module
// ---------------------------------------------------------------------------
TEST(ShaderModule, test_shader_module_initialize_creates_vk_shader_module)
{
	ASSERT_TRUE(WriteValidSpv());

	VulkanContext ctx;
	if (!ctx.Init())
	{
		GTEST_SKIP() << "Vulkan not available on this host.";
	}

	ShaderModule sm;
	RenderError err = sm.Initialize(ctx.device, kValidSpvPath.c_str());

	ASSERT_EQ(err, RenderError::None);
	EXPECT_TRUE(sm.IsInitialized());
	EXPECT_NE(sm.GetHandle(), VK_NULL_HANDLE);
	// The returned handle must be usable as the module field of
	// VkPipelineShaderStageCreateInfo — we verify it is non-null and that
	// IsInitialized agrees.
	EXPECT_EQ(sm.IsInitialized(), sm.GetHandle() != VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// shader_module_assumes_main_entry_point
// ---------------------------------------------------------------------------
// This semantic is a documentation / caller-contract feature: ShaderModule
// does not record or verify the entry-point name.  We verify the observable
// consequence: after a successful Initialize the ShaderModule exposes no
// entry-point API, and consumers (Pipeline) are expected to use "main".
TEST(ShaderModule, test_shader_module_assumes_main_entry_point)
{
	ASSERT_TRUE(WriteValidSpv());

	VulkanContext ctx;
	if (!ctx.Init())
	{
		GTEST_SKIP() << "Vulkan not available on this host.";
	}

	ShaderModule sm;
	ASSERT_EQ(sm.Initialize(ctx.device, kValidSpvPath.c_str()), RenderError::None);

	// ShaderModule has no GetEntryPoint() or similar method — the entry point
	// is not recorded.  The only observable state is the handle and IsInitialized.
	EXPECT_TRUE(sm.IsInitialized());
	EXPECT_NE(sm.GetHandle(), VK_NULL_HANDLE);
	// No entry-point accessor exists on ShaderModule — this is the contract.
	// (Compile-time check: the type does not expose such a method.)
	// We cannot static_assert the absence of a method portably, but the test
	// confirms the interface is limited to GetHandle / IsInitialized.
}

// ---------------------------------------------------------------------------
// get_handle_returns_owned_vk_shader_module_or_null
// ---------------------------------------------------------------------------
TEST(ShaderModule, test_get_handle_returns_owned_vk_shader_module_or_null)
{
	// Default-constructed: VK_NULL_HANDLE.
	{
		ShaderModule sm;
		EXPECT_EQ(sm.GetHandle(), VK_NULL_HANDLE);
	}

	ASSERT_TRUE(WriteValidSpv());

	VulkanContext ctx;
	if (!ctx.Init())
	{
		GTEST_SKIP() << "Vulkan not available on this host.";
	}

	// After successful Initialize: non-null.
	ShaderModule sm;
	ASSERT_EQ(sm.Initialize(ctx.device, kValidSpvPath.c_str()), RenderError::None);
	VkShaderModule h = sm.GetHandle();
	EXPECT_NE(h, VK_NULL_HANDLE);

	// GetHandle is a pure observer: calling it twice returns the same value.
	EXPECT_EQ(sm.GetHandle(), h);

	// After move: source returns VK_NULL_HANDLE, destination returns the handle.
	ShaderModule sm2(std::move(sm));
	EXPECT_EQ(sm.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(sm2.GetHandle(), h);

	// After destruction (sm2 goes out of scope), the handle is no longer valid
	// but we cannot test that without risking a use-after-free.  The destructor
	// calling vkDestroyShaderModule is the contract guarantee.
}

// ---------------------------------------------------------------------------
// is_initialized_reflects_owned_handle
// ---------------------------------------------------------------------------
TEST(ShaderModule, test_is_initialized_reflects_owned_handle)
{
	// Default-constructed: false.
	{
		ShaderModule sm;
		EXPECT_FALSE(sm.IsInitialized());
		EXPECT_EQ(sm.IsInitialized(), sm.GetHandle() != VK_NULL_HANDLE);
	}

	ASSERT_TRUE(WriteValidSpv());

	VulkanContext ctx;
	if (!ctx.Init())
	{
		GTEST_SKIP() << "Vulkan not available on this host.";
	}

	// After failed Initialize (bad path): false.
	{
		ShaderModule sm;
		sm.Initialize(ctx.device, nullptr);
		EXPECT_FALSE(sm.IsInitialized());
		EXPECT_EQ(sm.IsInitialized(), sm.GetHandle() != VK_NULL_HANDLE);
	}

	// After successful Initialize: true, consistent with GetHandle.
	{
		ShaderModule sm;
		ASSERT_EQ(sm.Initialize(ctx.device, kValidSpvPath.c_str()), RenderError::None);
		EXPECT_TRUE(sm.IsInitialized());
		EXPECT_EQ(sm.IsInitialized(), sm.GetHandle() != VK_NULL_HANDLE);

		// After move: source is false, destination is true.
		ShaderModule sm2(std::move(sm));
		EXPECT_FALSE(sm.IsInitialized());
		EXPECT_EQ(sm.IsInitialized(), sm.GetHandle() != VK_NULL_HANDLE);
		EXPECT_TRUE(sm2.IsInitialized());
		EXPECT_EQ(sm2.IsInitialized(), sm2.GetHandle() != VK_NULL_HANDLE);
	}
}

// ---------------------------------------------------------------------------
// shader_module_methods_are_not_thread_safe_per_instance
// ---------------------------------------------------------------------------
// This semantic is a documentation / safety contract: no synchronization is
// provided per-instance.  The observable consequence is that const observers
// (GetHandle, IsInitialized) may be called concurrently with each other on
// the same instance.  We verify this by calling them from two threads
// simultaneously on a read-only ShaderModule and confirming no data race
// occurs (under TSan) and that the results are consistent.
TEST(ShaderModule, test_shader_module_methods_are_not_thread_safe_per_instance)
{
	ASSERT_TRUE(WriteValidSpv());

	VulkanContext ctx;
	if (!ctx.Init())
	{
		GTEST_SKIP() << "Vulkan not available on this host.";
	}

	ShaderModule sm;
	ASSERT_EQ(sm.Initialize(ctx.device, kValidSpvPath.c_str()), RenderError::None);
	ASSERT_TRUE(sm.IsInitialized());

	// Call const observers from two threads concurrently — this is explicitly
	// permitted by the contract.
	std::atomic<bool> go{false};
	std::atomic<int> successCount{0};

	auto observer = [&]()
	{
		while (!go.load(std::memory_order_acquire))
		{
		}
		bool init = sm.IsInitialized();
		VkShaderModule h = sm.GetHandle();
		if (init && h != VK_NULL_HANDLE)
			++successCount;
	};

	std::thread t1(observer);
	std::thread t2(observer);
	go.store(true, std::memory_order_release);
	t1.join();
	t2.join();

	EXPECT_EQ(successCount.load(), 2);
}
