#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <type_traits>

#include "virasa/core/Logger.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/resources/Buffer.h"
#include "virasa/window/Platform.h"
#include "vulkan/vulkan.h"

using namespace virasa;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
namespace
{

// Bring up a real Vulkan Instance + Device so tests can exercise the real
// Vulkan path.  We use a headless surface (VK_NULL_HANDLE) which is accepted
// by the Device initializer when no present queue is strictly required.
struct VulkanFixture
{
	Instance instance;
	Device device;
	bool valid = false;

	VulkanFixture()
	{
		Logger::Initialize();

		RendererConfig cfg;
		cfg.enableValidation = false; // keep tests fast
		if (instance.Initialize(cfg) != RenderError::None)
			return;

		// Pass VK_NULL_HANDLE as surface – Device must still be able to
		// select a physical device for non-present workloads.
		if (device.Initialize(instance, VK_NULL_HANDLE) != RenderError::None)
			return;

		valid = true;
	}

	~VulkanFixture()
	{
		device.WaitIdle();
	}
};

// Build a host-visible, host-coherent BufferConfig suitable for Map/Upload/Write.
BufferConfig MakeHostVisibleConfig(VkDeviceSize size)
{
	BufferConfig cfg;
	cfg.size = size;
	cfg.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	cfg.memoryProperties =
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	return cfg;
}

// Build a device-local BufferConfig suitable for UploadViaStaging.
BufferConfig MakeDeviceLocalConfig(VkDeviceSize size)
{
	BufferConfig cfg;
	cfg.size = size;
	cfg.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	cfg.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	return cfg;
}

} // namespace

// ---------------------------------------------------------------------------
// buffer_is_raii_movable_non_copyable
// ---------------------------------------------------------------------------
TEST(Buffer, test_buffer_is_raii_movable_non_copyable)
{
	// Static type-trait checks (compile-time).
	static_assert(!std::is_copy_constructible_v<Buffer>, "Buffer must not be copy-constructible");
	static_assert(!std::is_copy_assignable_v<Buffer>, "Buffer must not be copy-assignable");
	static_assert(std::is_move_constructible_v<Buffer>, "Buffer must be move-constructible");
	static_assert(std::is_move_assignable_v<Buffer>, "Buffer must be move-assignable");
	static_assert(
		std::is_default_constructible_v<Buffer>, "Buffer must be default-constructible");
	static_assert(std::is_final_v<Buffer>, "Buffer must be final");

	VulkanFixture fx;
	if (!fx.valid)
		GTEST_SKIP() << "Vulkan device unavailable; skipping runtime move test";

	// Initialize a buffer so it owns real Vulkan resources.
	Buffer src;
	RenderError err = src.Initialize(fx.device, MakeHostVisibleConfig(256));
	ASSERT_EQ(err, RenderError::None);
	ASSERT_TRUE(src.IsInitialized());
	VkBuffer srcHandle = src.GetHandle();
	ASSERT_NE(srcHandle, VK_NULL_HANDLE);

	// Move-construct.
	Buffer dst(std::move(src));

	// Destination owns the resources.
	EXPECT_TRUE(dst.IsInitialized());
	EXPECT_EQ(dst.GetHandle(), srcHandle);
	EXPECT_EQ(dst.GetSize(), 256u);

	// Source is in default-constructed state.
	EXPECT_FALSE(src.IsInitialized());
	EXPECT_EQ(src.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(src.GetSize(), 0u);

	// Move-assign: dst2 starts empty, receives dst's resources.
	Buffer dst2;
	dst2 = std::move(dst);
	EXPECT_TRUE(dst2.IsInitialized());
	EXPECT_EQ(dst2.GetHandle(), srcHandle);
	EXPECT_FALSE(dst.IsInitialized());
	EXPECT_EQ(dst.GetHandle(), VK_NULL_HANDLE);

	// Move-assign into an already-initialized buffer (must teardown first).
	Buffer dst3;
	err = dst3.Initialize(fx.device, MakeHostVisibleConfig(128));
	ASSERT_EQ(err, RenderError::None);
	VkBuffer dst3OldHandle = dst3.GetHandle();
	ASSERT_NE(dst3OldHandle, VK_NULL_HANDLE);

	dst3 = std::move(dst2); // dst3 should teardown its own resources first.
	EXPECT_TRUE(dst3.IsInitialized());
	EXPECT_EQ(dst3.GetHandle(), srcHandle);
	EXPECT_FALSE(dst2.IsInitialized());

	// Destroying a moved-from buffer is well-defined (no crash).
	{
		Buffer movedFrom(std::move(dst3));
		(void)movedFrom;
	}
}

// ---------------------------------------------------------------------------
// buffer_default_constructed_state
// ---------------------------------------------------------------------------
TEST(Buffer, test_buffer_default_constructed_state)
{
	Buffer b;
	EXPECT_FALSE(b.IsInitialized());
	EXPECT_EQ(b.GetHandle(), VK_NULL_HANDLE);
	// GetSize is not pinned on non-initialized buffers, but a default-
	// constructed buffer must have size 0.
	EXPECT_EQ(b.GetSize(), 0u);

	// Destroying a default-constructed buffer is well-defined (no crash).
	// The destructor runs when 'b' goes out of scope.
}

// ---------------------------------------------------------------------------
// buffer_config_describes_buffer_creation_parameters
// ---------------------------------------------------------------------------
TEST(Buffer, test_buffer_config_describes_buffer_creation_parameters)
{
	// Static type-trait checks.
	static_assert(std::is_copy_constructible_v<BufferConfig>);
	static_assert(std::is_move_constructible_v<BufferConfig>);
	static_assert(std::is_default_constructible_v<BufferConfig>);

	// Default values.
	BufferConfig cfg;
	EXPECT_EQ(cfg.size, static_cast<VkDeviceSize>(0));
	EXPECT_EQ(cfg.usage, static_cast<VkBufferUsageFlags>(0));
	EXPECT_EQ(cfg.memoryProperties,
		static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

	// Members are assignable and readable.
	cfg.size = 1024;
	cfg.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	cfg.memoryProperties =
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	EXPECT_EQ(cfg.size, 1024u);
	EXPECT_EQ(cfg.usage, static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
	EXPECT_EQ(cfg.memoryProperties,
		static_cast<VkMemoryPropertyFlags>(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

	// Copy.
	BufferConfig copy = cfg;
	EXPECT_EQ(copy.size, cfg.size);
	EXPECT_EQ(copy.usage, cfg.usage);
	EXPECT_EQ(copy.memoryProperties, cfg.memoryProperties);
}

// ---------------------------------------------------------------------------
// buffer_initialize_creates_buffer_and_memory
// ---------------------------------------------------------------------------
TEST(Buffer, test_buffer_initialize_creates_buffer_and_memory)
{
	VulkanFixture fx;
	if (!fx.valid)
		GTEST_SKIP() << "Vulkan device unavailable";

	// --- Success path ---
	Buffer buf;
	BufferConfig cfg = MakeHostVisibleConfig(512);
	RenderError err = buf.Initialize(fx.device, cfg);
	EXPECT_EQ(err, RenderError::None);
	EXPECT_TRUE(buf.IsInitialized());
	EXPECT_NE(buf.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(buf.GetSize(), 512u);

	// --- Re-initialization (teardown-then-recreate) ---
	VkBuffer firstHandle = buf.GetHandle();
	BufferConfig cfg2 = MakeHostVisibleConfig(1024);
	err = buf.Initialize(fx.device, cfg2);
	EXPECT_EQ(err, RenderError::None);
	EXPECT_TRUE(buf.IsInitialized());
	EXPECT_NE(buf.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(buf.GetSize(), 1024u);
	// The old handle should have been destroyed; the new one may or may not
	// reuse the same value — we only verify the buffer is valid.
	(void)firstHandle;

	// --- Failure path: zero-size (driver should reject) ---
	// The contract says behavior is not pinned for zero-size, but we verify
	// that on failure IsInitialized is false and GetHandle is VK_NULL_HANDLE.
	Buffer bad;
	BufferConfig badCfg;
	badCfg.size = 0;
	badCfg.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	badCfg.memoryProperties =
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	RenderError badErr = bad.Initialize(fx.device, badCfg);
	if (badErr != RenderError::None)
	{
		// Failure transactionality: must be left empty.
		EXPECT_FALSE(bad.IsInitialized());
		EXPECT_EQ(bad.GetHandle(), VK_NULL_HANDLE);
	}
	// (If the driver accepts size 0 we simply skip the failure assertions.)
}

// ---------------------------------------------------------------------------
// buffer_memory_type_selection
// ---------------------------------------------------------------------------
TEST(Buffer, test_buffer_memory_type_selection)
{
	VulkanFixture fx;
	if (!fx.valid)
		GTEST_SKIP() << "Vulkan device unavailable";

	// Device-local buffer: selection must find DEVICE_LOCAL memory.
	Buffer deviceLocal;
	RenderError err = deviceLocal.Initialize(fx.device, MakeDeviceLocalConfig(256));
	EXPECT_EQ(err, RenderError::None);
	EXPECT_TRUE(deviceLocal.IsInitialized());

	// Host-visible + host-coherent buffer: selection must find HOST_VISIBLE memory.
	Buffer hostVisible;
	err = hostVisible.Initialize(fx.device, MakeHostVisibleConfig(256));
	EXPECT_EQ(err, RenderError::None);
	EXPECT_TRUE(hostVisible.IsInitialized());

	// Verify the selection algorithm by constructing an impossible property
	// combination.  We request a flag combination that no real device provides
	// (DEVICE_LOCAL | HOST_VISIBLE | LAZILY_ALLOCATED is extremely rare; if the
	// device does support it the test simply passes Initialize — we accept that).
	// The important thing is that when selection fails, Initialize returns
	// MemoryAllocFailed and leaves the buffer non-initialized.
	const VkMemoryPropertyFlags kImpossible = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
								VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
								VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;

	// Check whether any memory type actually satisfies this combination.
	const auto& memProps = fx.device.GetMemoryProperties();
	bool anyTypeMatches = false;
	for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
	{
		if ((memProps.memoryTypes[i].propertyFlags & kImpossible) == kImpossible)
		{
			anyTypeMatches = true;
			break;
		}
	}

	if (!anyTypeMatches)
	{
		BufferConfig impossibleCfg;
		impossibleCfg.size = 256;
		impossibleCfg.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		impossibleCfg.memoryProperties = kImpossible;

		Buffer impossible;
		err = impossible.Initialize(fx.device, impossibleCfg);
		EXPECT_EQ(err, RenderError::MemoryAllocFailed);
		EXPECT_FALSE(impossible.IsInitialized());
		EXPECT_EQ(impossible.GetHandle(), VK_NULL_HANDLE);
	}

	// Verify the first-fit ascending-index scan by inspecting the physical
	// device memory properties directly and confirming that the selected type
	// (inferred by the fact that Initialize succeeded) satisfies the requested
	// flags.  We cannot read the selected index from the Buffer, but we can
	// confirm that a matching type exists at some index.
	bool foundDeviceLocal = false;
	bool foundHostVisible = false;
	for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
	{
		const VkMemoryPropertyFlags f = memProps.memoryTypes[i].propertyFlags;
		if ((f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			foundDeviceLocal = true;
		if ((f & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
				   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
			(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			foundHostVisible = true;
	}
	EXPECT_TRUE(foundDeviceLocal);
	EXPECT_TRUE(foundHostVisible);
}

// ---------------------------------------------------------------------------
// buffer_map_unmap_persistent_mapping
// ---------------------------------------------------------------------------
TEST(Buffer, test_buffer_map_unmap_persistent_mapping)
{
	VulkanFixture fx;
	if (!fx.valid)
		GTEST_SKIP() << "Vulkan device unavailable";

	Buffer buf;
	ASSERT_EQ(buf.Initialize(fx.device, MakeHostVisibleConfig(256)), RenderError::None);

	// First Map call: should return a non-null pointer.
	void* ptr1 = buf.Map();
	ASSERT_NE(ptr1, nullptr);

	// Second Map call (persistent): should return the same cached pointer.
	void* ptr2 = buf.Map();
	EXPECT_EQ(ptr1, ptr2);

	// Unmap: releases the mapping.
	buf.Unmap();

	// After Unmap, a new Map call should succeed (may return same or different ptr).
	void* ptr3 = buf.Map();
	EXPECT_NE(ptr3, nullptr);

	// Unmap again.
	buf.Unmap();

	// Unmap on an already-unmapped buffer is a no-op (no crash).
	buf.Unmap();
}

// ---------------------------------------------------------------------------
// buffer_upload_writes_host_visible_memory
// ---------------------------------------------------------------------------
TEST(Buffer, test_buffer_upload_writes_host_visible_memory)
{
	VulkanFixture fx;
	if (!fx.valid)
		GTEST_SKIP() << "Vulkan device unavailable";

	constexpr VkDeviceSize kSize = 256;
	Buffer buf;
	ASSERT_EQ(buf.Initialize(fx.device, MakeHostVisibleConfig(kSize)), RenderError::None);

	// --- Upload ---
	uint8_t srcData[kSize];
	for (size_t i = 0; i < kSize; ++i)
		srcData[i] = static_cast<uint8_t>(i & 0xFF);

	RenderError err = buf.Upload(srcData, kSize);
	EXPECT_EQ(err, RenderError::None);

	// Verify the data was written by mapping and reading back.
	void* mapped = buf.Map();
	ASSERT_NE(mapped, nullptr);
	EXPECT_EQ(std::memcmp(mapped, srcData, kSize), 0);
	buf.Unmap();

	// --- Write (with offset) ---
	uint8_t patch[16];
	for (size_t i = 0; i < 16; ++i)
		patch[i] = 0xAB;

	err = buf.Write(patch, 16, 32);
	EXPECT_EQ(err, RenderError::None);

	mapped = buf.Map();
	ASSERT_NE(mapped, nullptr);
	EXPECT_EQ(std::memcmp(static_cast<uint8_t*>(mapped) + 32, patch, 16), 0);
	buf.Unmap();

	// --- Persistent-mapping interaction ---
	// Establish a persistent map before calling Upload; Unmap must NOT be
	// called by Upload (the mapping should still be live after Upload returns).
	void* persistentPtr = buf.Map();
	ASSERT_NE(persistentPtr, nullptr);

	uint8_t newData[kSize];
	std::memset(newData, 0x55, kSize);
	err = buf.Upload(newData, kSize);
	EXPECT_EQ(err, RenderError::None);

	// The mapping must still be live (same pointer).
	void* afterUpload = buf.Map();
	EXPECT_EQ(persistentPtr, afterUpload);
	buf.Unmap();
}

// ---------------------------------------------------------------------------
// buffer_upload_via_staging_for_device_local
// ---------------------------------------------------------------------------
TEST(Buffer, test_buffer_upload_via_staging_for_device_local)
{
	VulkanFixture fx;
	if (!fx.valid)
		GTEST_SKIP() << "Vulkan device unavailable";

	constexpr size_t kSize = 256;

	// Create a device-local destination buffer.
	Buffer dst;
	ASSERT_EQ(dst.Initialize(fx.device, MakeDeviceLocalConfig(kSize)), RenderError::None);

	// Build a transfer command pool on the transfer queue family.
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = fx.device.GetQueueFamilies().transferFamily;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	VkCommandPool transferPool = VK_NULL_HANDLE;
	VkResult vkRes =
		vkCreateCommandPool(fx.device.GetHandle(), &poolInfo, nullptr, &transferPool);
	ASSERT_EQ(vkRes, VK_SUCCESS);

	uint8_t srcData[kSize];
	for (size_t i = 0; i < kSize; ++i)
		srcData[i] = static_cast<uint8_t>(i & 0xFF);

	RenderError err = dst.UploadViaStaging(
		fx.device, transferPool, fx.device.GetTransferQueue(), srcData, kSize);
	EXPECT_EQ(err, RenderError::None);

	// The destination buffer must still be initialized after the upload.
	EXPECT_TRUE(dst.IsInitialized());
	EXPECT_NE(dst.GetHandle(), VK_NULL_HANDLE);

	vkDestroyCommandPool(fx.device.GetHandle(), transferPool, nullptr);
}

// ---------------------------------------------------------------------------
// buffer_observers_return_cached_state
// ---------------------------------------------------------------------------
TEST(Buffer, test_buffer_observers_return_cached_state)
{
	VulkanFixture fx;
	if (!fx.valid)
		GTEST_SKIP() << "Vulkan device unavailable";

	// Default-constructed: GetHandle == VK_NULL_HANDLE.
	Buffer buf;
	EXPECT_EQ(buf.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(buf.GetSize(), 0u);

	// After successful Initialize: observers reflect the config.
	constexpr VkDeviceSize kSize = 512;
	ASSERT_EQ(buf.Initialize(fx.device, MakeHostVisibleConfig(kSize)), RenderError::None);
	EXPECT_NE(buf.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(buf.GetSize(), kSize);

	// Capture handle before move.
	VkBuffer handle = buf.GetHandle();

	// After move-from: observers reflect moved-from (empty) state.
	Buffer other(std::move(buf));
	EXPECT_EQ(buf.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(buf.GetSize(), 0u);

	// Destination owns the handle.
	EXPECT_EQ(other.GetHandle(), handle);
	EXPECT_EQ(other.GetSize(), kSize);

	// GetHandle and GetSize are noexcept.
	static_assert(noexcept(other.GetHandle()));
	static_assert(noexcept(other.GetSize()));
}

// ---------------------------------------------------------------------------
// buffer_is_initialized_reflects_owned_handle
// ---------------------------------------------------------------------------
TEST(Buffer, test_buffer_is_initialized_reflects_owned_handle)
{
	VulkanFixture fx;
	if (!fx.valid)
		GTEST_SKIP() << "Vulkan device unavailable";

	Buffer buf;

	// Default-constructed: not initialized, handle is null.
	EXPECT_FALSE(buf.IsInitialized());
	EXPECT_EQ(buf.GetHandle(), VK_NULL_HANDLE);
	// Invariant: IsInitialized iff GetHandle != VK_NULL_HANDLE.
	EXPECT_EQ(buf.IsInitialized(), buf.GetHandle() != VK_NULL_HANDLE);

	// After successful Initialize.
	ASSERT_EQ(buf.Initialize(fx.device, MakeHostVisibleConfig(128)), RenderError::None);
	EXPECT_TRUE(buf.IsInitialized());
	EXPECT_NE(buf.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(buf.IsInitialized(), buf.GetHandle() != VK_NULL_HANDLE);

	// After move-from: not initialized.
	Buffer other(std::move(buf));
	EXPECT_FALSE(buf.IsInitialized());
	EXPECT_EQ(buf.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(buf.IsInitialized(), buf.GetHandle() != VK_NULL_HANDLE);

	// Destination is initialized.
	EXPECT_TRUE(other.IsInitialized());
	EXPECT_NE(other.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(other.IsInitialized(), other.GetHandle() != VK_NULL_HANDLE);

	// IsInitialized is noexcept.
	static_assert(noexcept(other.IsInitialized()));
}

// ---------------------------------------------------------------------------
// buffer_is_not_thread_safe_per_instance
// ---------------------------------------------------------------------------
// This semantic is a documentation/contract statement about the absence of
// internal synchronization.  We verify the structural claim (distinct Buffer
// objects can be used independently) and the const-observer concurrency
// guarantee by calling const observers from multiple threads on the same
// object.  We do NOT attempt concurrent mutation on the same object (that
// would be UB).
TEST(Buffer, test_buffer_is_not_thread_safe_per_instance)
{
	VulkanFixture fx;
	if (!fx.valid)
		GTEST_SKIP() << "Vulkan device unavailable";

	Buffer buf;
	ASSERT_EQ(buf.Initialize(fx.device, MakeHostVisibleConfig(64)), RenderError::None);

	// Concurrent const-observer calls on the same initialized Buffer are safe.
	// We spin up two threads that each call the const observers many times.
	std::atomic<bool> go{false};
	std::atomic<int> errors{0};

	auto readTask = [&]()
	{
		while (!go.load(std::memory_order_acquire))
		{
		}
		for (int i = 0; i < 1000; ++i)
		{
			if (buf.GetHandle() == VK_NULL_HANDLE)
				++errors;
			if (!buf.IsInitialized())
				++errors;
			if (buf.GetSize() != 64u)
				++errors;
		}
	};

	std::thread t1(readTask);
	std::thread t2(readTask);
	go.store(true, std::memory_order_release);
	t1.join();
	t2.join();

	EXPECT_EQ(errors.load(), 0);

	// Distinct Buffer objects on different threads: each initializes and
	// uses its own Buffer independently.
	std::atomic<RenderError> err1{RenderError::None};
	std::atomic<RenderError> err2{RenderError::None};

	std::thread t3(
		[&]()
		{
			Buffer b;
			err1.store(b.Initialize(fx.device, MakeHostVisibleConfig(64)));
		});
	std::thread t4(
		[&]()
		{
			Buffer b;
			err2.store(b.Initialize(fx.device, MakeHostVisibleConfig(64)));
		});
	t3.join();
	t4.join();

	EXPECT_EQ(err1.load(), RenderError::None);
	EXPECT_EQ(err2.load(), RenderError::None);
}
