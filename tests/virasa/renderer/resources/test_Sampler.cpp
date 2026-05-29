#include <gtest/gtest.h>
#include <thread>
#include <type_traits>
#include <utility>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/resources/Sampler.h"
#include "virasa/window/Platform.h"
#include "vulkan/vulkan.h"

using namespace virasa;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{

// Build a minimal RendererConfig suitable for headless / no-window use.
RendererConfig MakeHeadlessConfig()
{
	RendererConfig cfg;
	cfg.applicationName = "SamplerTest";
	cfg.enableValidation = false;
	cfg.requiredInstanceExtensions = nullptr;
	cfg.requiredInstanceExtensionCount = 0;
	return cfg;
}

// Attempt to bring up a real Instance + Device for integration tests.
// Returns false if the environment cannot support Vulkan (CI without a GPU).
struct RealDevice
{
	Instance instance;
	Device device;
	bool valid = false;

	RealDevice()
	{
		if (instance.Initialize(MakeHeadlessConfig()) != RenderError::None)
			return;
		// A headless device needs no surface; pass VK_NULL_HANDLE.
		if (device.Initialize(instance, VK_NULL_HANDLE) != RenderError::None)
			return;
		valid = true;
	}
};

} // namespace

// ---------------------------------------------------------------------------
// TEST: sampler_default_constructed_state
// ---------------------------------------------------------------------------
TEST(Sampler, test_sampler_default_constructed_state)
{
	Sampler s;
	EXPECT_FALSE(s.IsInitialized());
	EXPECT_EQ(s.GetHandle(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// TEST: sampler_is_raii_movable_non_copyable
// ---------------------------------------------------------------------------
TEST(Sampler, test_sampler_is_raii_movable_non_copyable)
{
	// Non-copyable
	EXPECT_FALSE((std::is_copy_constructible_v<Sampler>));
	EXPECT_FALSE((std::is_copy_assignable_v<Sampler>));

	// Movable
	EXPECT_TRUE((std::is_move_constructible_v<Sampler>));
	EXPECT_TRUE((std::is_move_assignable_v<Sampler>));

	// Default-constructible
	EXPECT_TRUE((std::is_default_constructible_v<Sampler>));

	// Final
	EXPECT_TRUE((std::is_final_v<Sampler>));

	// Move constructor transfers ownership; source becomes default-constructed state.
	{
		Sampler a;
		// a is not initialized — move it anyway; both sides should be null.
		Sampler b(std::move(a));
		EXPECT_FALSE(a.IsInitialized());
		EXPECT_EQ(a.GetHandle(), VK_NULL_HANDLE);
		EXPECT_FALSE(b.IsInitialized());
		EXPECT_EQ(b.GetHandle(), VK_NULL_HANDLE);
	}

	// Move assignment from default-constructed source.
	{
		Sampler a;
		Sampler b;
		b = std::move(a);
		EXPECT_FALSE(a.IsInitialized());
		EXPECT_EQ(a.GetHandle(), VK_NULL_HANDLE);
		EXPECT_FALSE(b.IsInitialized());
		EXPECT_EQ(b.GetHandle(), VK_NULL_HANDLE);
	}

	// Destroying a default-constructed Sampler is well-defined (no crash).
	{
		Sampler s;
		// destructor runs at end of scope — no crash expected.
	}

	// Destroying a moved-from Sampler is well-defined.
	{
		Sampler a;
		Sampler b(std::move(a));
		// Both destructors run at end of scope — no crash expected.
	}

	// If a real device is available, test move with an initialized Sampler.
	RealDevice rd;
	if (!rd.valid)
		GTEST_SKIP() << "No Vulkan device available; skipping initialized-move sub-test.";

	SamplerConfig cfg;
	Sampler src;
	ASSERT_EQ(src.Initialize(rd.device, cfg), RenderError::None);
	VkSampler originalHandle = src.GetHandle();
	ASSERT_NE(originalHandle, VK_NULL_HANDLE);

	// Move construct
	Sampler dst(std::move(src));
	EXPECT_FALSE(src.IsInitialized());
	EXPECT_EQ(src.GetHandle(), VK_NULL_HANDLE);
	EXPECT_TRUE(dst.IsInitialized());
	EXPECT_EQ(dst.GetHandle(), originalHandle);

	// Move assign into an already-initialized Sampler (should destroy dst's handle first).
	Sampler src2;
	ASSERT_EQ(src2.Initialize(rd.device, cfg), RenderError::None);
	VkSampler handle2 = src2.GetHandle();
	ASSERT_NE(handle2, VK_NULL_HANDLE);

	dst = std::move(src2);
	EXPECT_FALSE(src2.IsInitialized());
	EXPECT_EQ(src2.GetHandle(), VK_NULL_HANDLE);
	EXPECT_TRUE(dst.IsInitialized());
	EXPECT_EQ(dst.GetHandle(), handle2);
}

// ---------------------------------------------------------------------------
// TEST: sampler_initialize_creates_vk_sampler
// ---------------------------------------------------------------------------
TEST(Sampler, test_sampler_initialize_creates_vk_sampler)
{
	RealDevice rd;
	if (!rd.valid)
		GTEST_SKIP() << "No Vulkan device available; skipping Initialize test.";

	// Basic initialization with default config.
	Sampler s;
	SamplerConfig cfg;
	RenderError err = s.Initialize(rd.device, cfg);
	EXPECT_EQ(err, RenderError::None);
	EXPECT_TRUE(s.IsInitialized());
	EXPECT_NE(s.GetHandle(), VK_NULL_HANDLE);

	// Re-initialization: Initialize on an already-initialized Sampler should
	// destroy the old handle and create a new one without leaking.
	VkSampler firstHandle = s.GetHandle();
	SamplerConfig cfg2;
	cfg2.magFilter = VK_FILTER_NEAREST;
	cfg2.minFilter = VK_FILTER_NEAREST;
	cfg2.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	RenderError err2 = s.Initialize(rd.device, cfg2);
	EXPECT_EQ(err2, RenderError::None);
	EXPECT_TRUE(s.IsInitialized());
	EXPECT_NE(s.GetHandle(), VK_NULL_HANDLE);
	// The handle may or may not be the same value (driver may reuse it),
	// but initialization must succeed and the sampler must be valid.
	(void)firstHandle;

	// Verify a non-default config round-trip: clamp-to-edge address modes.
	Sampler s2;
	SamplerConfig cfg3;
	cfg3.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cfg3.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cfg3.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cfg3.minLod = 0.0f;
	cfg3.maxLod = 4.0f;
	cfg3.mipLodBias = 0.5f;
	EXPECT_EQ(s2.Initialize(rd.device, cfg3), RenderError::None);
	EXPECT_TRUE(s2.IsInitialized());
	EXPECT_NE(s2.GetHandle(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// TEST: sampler_observers_return_owned_handle
// ---------------------------------------------------------------------------
TEST(Sampler, test_sampler_observers_return_owned_handle)
{
	// Default-constructed: GetHandle == VK_NULL_HANDLE, IsInitialized == false.
	Sampler s;
	EXPECT_EQ(s.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(s.IsInitialized());

	// The invariant: IsInitialized() iff GetHandle() != VK_NULL_HANDLE.
	// For a default-constructed sampler both are consistent.
	EXPECT_EQ(s.IsInitialized(), s.GetHandle() != VK_NULL_HANDLE);

	RealDevice rd;
	if (!rd.valid)
		GTEST_SKIP() << "No Vulkan device available; skipping initialized-observer sub-test.";

	SamplerConfig cfg;
	ASSERT_EQ(s.Initialize(rd.device, cfg), RenderError::None);

	// After successful Initialize: IsInitialized true, GetHandle non-null.
	EXPECT_TRUE(s.IsInitialized());
	EXPECT_NE(s.GetHandle(), VK_NULL_HANDLE);

	// Invariant holds after initialization.
	EXPECT_EQ(s.IsInitialized(), s.GetHandle() != VK_NULL_HANDLE);

	// GetHandle is stable across multiple calls (pure observer).
	VkSampler h1 = s.GetHandle();
	VkSampler h2 = s.GetHandle();
	EXPECT_EQ(h1, h2);

	// After move, source returns to null / false state.
	Sampler dst(std::move(s));
	EXPECT_FALSE(s.IsInitialized());
	EXPECT_EQ(s.GetHandle(), VK_NULL_HANDLE);
	EXPECT_EQ(s.IsInitialized(), s.GetHandle() != VK_NULL_HANDLE);

	// Destination holds the handle.
	EXPECT_TRUE(dst.IsInitialized());
	EXPECT_EQ(dst.GetHandle(), h1);
	EXPECT_EQ(dst.IsInitialized(), dst.GetHandle() != VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// TEST: sampler_is_not_thread_safe_per_instance
// ---------------------------------------------------------------------------
// The contract specifies that distinct Sampler objects may be used concurrently
// from different threads. We verify this by creating two independent Samplers
// on separate threads and confirming each ends up in the expected state.
// We do NOT exercise concurrent mutation of the same object (that is UB per
// the contract and cannot be tested safely).
TEST(Sampler, test_sampler_is_not_thread_safe_per_instance)
{
	// Verify the type-trait side: const observers are noexcept.
	EXPECT_TRUE(noexcept(std::declval<const Sampler&>().GetHandle()));
	EXPECT_TRUE(noexcept(std::declval<const Sampler&>().IsInitialized()));

	RealDevice rd;
	if (!rd.valid)
		GTEST_SKIP() << "No Vulkan device available; skipping cross-thread sub-test.";

	// Two distinct Sampler objects initialized from different threads.
	Sampler s1, s2;
	RenderError err1 = RenderError::SamplerCreateFailed;
	RenderError err2 = RenderError::SamplerCreateFailed;

// Use std::thread to initialize each sampler independently.
	std::thread t1(
		[&]()
		{
			SamplerConfig cfg;
			err1 = s1.Initialize(rd.device, cfg);
		});
	std::thread t2(
		[&]()
		{
			SamplerConfig cfg;
			err2 = s2.Initialize(rd.device, cfg);
		});
	t1.join();
	t2.join();

	EXPECT_EQ(err1, RenderError::None);
	EXPECT_EQ(err2, RenderError::None);
	EXPECT_TRUE(s1.IsInitialized());
	EXPECT_TRUE(s2.IsInitialized());
	EXPECT_NE(s1.GetHandle(), VK_NULL_HANDLE);
	EXPECT_NE(s2.GetHandle(), VK_NULL_HANDLE);
}
