#include <gtest/gtest.h>

#include "virasa/renderer/lighting/ShadowTable.h"

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/resources/Buffer.h"
#include "virasa/math/Types.h"

#include "vulkan/vulkan.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <utility>

using namespace virasa;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{

// Build a real Device backed by a real Vulkan instance (no surface — headless),
// mirroring the LightTable test harness. On failure the test should be skipped.
struct TestContext
{
	Instance instance;
	Device device;
	bool valid = false;

	TestContext()
	{
		RendererConfig cfg{};
		cfg.applicationName = "ShadowTableTest";
		cfg.enableValidation = false;
		cfg.requiredInstanceExtensions = nullptr;
		cfg.requiredInstanceExtensionCount = 0;

		if (instance.Initialize(cfg) != RenderError::None)
			return;

		// No surface — pass VK_NULL_HANDLE. Device selection may fail if the
		// physical device requires a present queue; we accept that in CI.
		if (device.Initialize(instance, VK_NULL_HANDLE) != RenderError::None)
			return;

		valid = true;
	}
};

} // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// shadow_gpu_layout_is_80_bytes_scalar_compatible
TEST(ShadowTable, test_shadow_gpu_layout_is_80_bytes_scalar_compatible)
{
	// Size must be exactly 80 bytes.
	EXPECT_EQ(sizeof(ShadowGPU), 80u);

	// Verify byte offsets of each member.
	EXPECT_EQ(offsetof(ShadowGPU, lightViewProj), 0u);
	EXPECT_EQ(offsetof(ShadowGPU, shadowMapSlot), 64u);
	EXPECT_EQ(offsetof(ShadowGPU, depthBias),     68u);
	EXPECT_EQ(offsetof(ShadowGPU, slopeBias),     72u);
	EXPECT_EQ(offsetof(ShadowGPU, _pad0),         76u);

	// Verify member sizes.
	EXPECT_EQ(sizeof(ShadowGPU::lightViewProj), 64u);
	EXPECT_EQ(sizeof(ShadowGPU::shadowMapSlot),  4u);
	EXPECT_EQ(sizeof(ShadowGPU::depthBias),      4u);
	EXPECT_EQ(sizeof(ShadowGPU::slopeBias),      4u);
	EXPECT_EQ(sizeof(ShadowGPU::_pad0),          4u);

	// Verify default values.
	ShadowGPU s;
	math::Mat4 identity(1.0f);
	EXPECT_EQ(std::memcmp(&s.lightViewProj, &identity, sizeof(identity)), 0);
	EXPECT_EQ(s.shadowMapSlot, 0u);
	EXPECT_NEAR(s.depthBias, 0.0015f, 1e-7f);
	EXPECT_NEAR(s.slopeBias, 0.0025f, 1e-7f);
	EXPECT_EQ(s._pad0,         0u);

	// Verify copyable, movable, default-constructible, trivially destructible.
	EXPECT_TRUE((std::is_copy_constructible_v<ShadowGPU>));
	EXPECT_TRUE((std::is_copy_assignable_v<ShadowGPU>));
	EXPECT_TRUE((std::is_move_constructible_v<ShadowGPU>));
	EXPECT_TRUE((std::is_move_assignable_v<ShadowGPU>));
	EXPECT_TRUE((std::is_default_constructible_v<ShadowGPU>));
	EXPECT_TRUE((std::is_trivially_destructible_v<ShadowGPU>));
}

// shadow_table_is_raii_movable_non_copyable
TEST(ShadowTable, test_shadow_table_is_raii_movable_non_copyable)
{
	// Not copyable.
	EXPECT_FALSE((std::is_copy_constructible_v<ShadowTable>));
	EXPECT_FALSE((std::is_copy_assignable_v<ShadowTable>));

	// Movable.
	EXPECT_TRUE((std::is_move_constructible_v<ShadowTable>));
	EXPECT_TRUE((std::is_move_assignable_v<ShadowTable>));

	// Default-constructible.
	EXPECT_TRUE((std::is_default_constructible_v<ShadowTable>));

	// Final.
	EXPECT_TRUE((std::is_final_v<ShadowTable>));

	// Move constructor transfers state; source becomes empty.
	TestContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan device unavailable; skipping move-with-resources test";
	}

	constexpr uint32_t kFif = 2u;

	ShadowTable a;
	ASSERT_EQ(a.Initialize(ctx.device, 4u, kFif), RenderError::None);
	EXPECT_TRUE(a.IsInitialized());
	EXPECT_EQ(a.GetCapacity(), 4u);
	VkDeviceAddress addrA = a.GetBufferAddress();
	EXPECT_NE(addrA, 0u);

	ShadowTable b(std::move(a));
	EXPECT_TRUE(b.IsInitialized());
	EXPECT_EQ(b.GetCapacity(), 4u);
	EXPECT_EQ(b.GetBufferAddress(), addrA);

	// Source must be empty after move.
	EXPECT_FALSE(a.IsInitialized());
	EXPECT_EQ(a.GetCapacity(), 0u);
	EXPECT_EQ(a.GetShadowCount(), 0u);
	EXPECT_EQ(a.GetBufferAddress(), 0u);

	// Move assignment: assign into an already-initialized table.
	ShadowTable c;
	ASSERT_EQ(c.Initialize(ctx.device, 8u, kFif), RenderError::None);
	EXPECT_TRUE(c.IsInitialized());

	ShadowTable d;
	ASSERT_EQ(d.Initialize(ctx.device, 2u, kFif), RenderError::None);
	VkDeviceAddress addrD = d.GetBufferAddress();

	c = std::move(d);
	EXPECT_TRUE(c.IsInitialized());
	EXPECT_EQ(c.GetCapacity(), 2u);
	EXPECT_EQ(c.GetBufferAddress(), addrD);

	// Source must be empty after move-assignment.
	EXPECT_FALSE(d.IsInitialized());
	EXPECT_EQ(d.GetCapacity(), 0u);
	EXPECT_EQ(d.GetShadowCount(), 0u);
	EXPECT_EQ(d.GetBufferAddress(), 0u);
}

// shadow_table_default_constructed_state
TEST(ShadowTable, test_shadow_table_default_constructed_state)
{
	ShadowTable t;
	EXPECT_FALSE(t.IsInitialized());
	EXPECT_EQ(t.GetCapacity(), 0u);
	EXPECT_EQ(t.GetShadowCount(), 0u);
	EXPECT_EQ(t.GetBufferAddress(), 0u);
}

// shadow_table_initialize_creates_host_visible_buffer
TEST(ShadowTable, test_shadow_table_initialize_creates_host_visible_buffer)
{
	TestContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan device unavailable";
	}

	ShadowTable t;

	// Pre-init state.
	EXPECT_FALSE(t.IsInitialized());
	EXPECT_EQ(t.GetCapacity(), 0u);
	EXPECT_EQ(t.GetShadowCount(), 0u);
	EXPECT_EQ(t.GetBufferAddress(), 0u);

	constexpr uint32_t kMaxShadows = 16u;
	constexpr uint32_t kFramesInFlight = 2u;
	RenderError err = t.Initialize(ctx.device, kMaxShadows, kFramesInFlight);
	ASSERT_EQ(err, RenderError::None);

	EXPECT_TRUE(t.IsInitialized());
	EXPECT_EQ(t.GetCapacity(), kMaxShadows);
	EXPECT_EQ(t.GetShadowCount(), 0u);
	EXPECT_NE(t.GetBufferAddress(), 0u);

	// Current-frame index is 0 immediately after Initialize.
	// GetBufferAddress returns ring entry 0's address — just verify it's non-zero.
	VkDeviceAddress addrAfterInit = t.GetBufferAddress();
	EXPECT_NE(addrAfterInit, 0u);

	// Re-initialization must not leak: call Initialize again and verify it succeeds.
	constexpr uint32_t kNewMax = 8u;
	err = t.Initialize(ctx.device, kNewMax, kFramesInFlight);
	ASSERT_EQ(err, RenderError::None);
	EXPECT_TRUE(t.IsInitialized());
	EXPECT_EQ(t.GetCapacity(), kNewMax);
	EXPECT_EQ(t.GetShadowCount(), 0u);
	EXPECT_NE(t.GetBufferAddress(), 0u);

	// Single frames_in_flight=1 should behave like version-1 (one buffer).
	ShadowTable t1;
	err = t1.Initialize(ctx.device, kMaxShadows, 1u);
	ASSERT_EQ(err, RenderError::None);
	EXPECT_TRUE(t1.IsInitialized());
	EXPECT_EQ(t1.GetCapacity(), kMaxShadows);
	EXPECT_NE(t1.GetBufferAddress(), 0u);
}

// shadow_table_upload_frame_writes_compact_array
TEST(ShadowTable, test_shadow_table_upload_frame_writes_compact_array)
{
	TestContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan device unavailable";
	}

	constexpr uint32_t kCapacity = 4u;
	constexpr uint32_t kFif = 2u;
	ShadowTable t;
	ASSERT_EQ(t.Initialize(ctx.device, kCapacity, kFif), RenderError::None);

	// Upload zero records: count becomes 0, returns 0.
	{
		std::span<const ShadowGPU> empty;
		uint32_t written = t.UploadFrame(empty);
		EXPECT_EQ(written, 0u);
		EXPECT_EQ(t.GetShadowCount(), 0u);
	}

	// Upload exactly capacity records: all written.
	{
		std::array<ShadowGPU, kCapacity> shadows{};
		for (uint32_t i = 0; i < kCapacity; ++i)
			shadows[i].shadowMapSlot = i;

		uint32_t written = t.UploadFrame(std::span<const ShadowGPU>(shadows));
		EXPECT_EQ(written, kCapacity);
		EXPECT_EQ(t.GetShadowCount(), kCapacity);
	}

	// Upload fewer than capacity: only that many written.
	{
		constexpr uint32_t kFew = 2u;
		std::array<ShadowGPU, kFew> shadows{};
		shadows[0].shadowMapSlot = 10u;
		shadows[1].shadowMapSlot = 11u;

		uint32_t written = t.UploadFrame(std::span<const ShadowGPU>(shadows));
		EXPECT_EQ(written, kFew);
		EXPECT_EQ(t.GetShadowCount(), kFew);
	}

	// Upload more than capacity: clamped to capacity, excess dropped (warning logged).
	{
		constexpr uint32_t kExcess = kCapacity + 3u;
		std::array<ShadowGPU, kExcess> shadows{};
		for (uint32_t i = 0; i < kExcess; ++i)
			shadows[i].shadowMapSlot = i;

		uint32_t written = t.UploadFrame(std::span<const ShadowGPU>(shadows));
		EXPECT_EQ(written, kCapacity);
		EXPECT_EQ(t.GetShadowCount(), kCapacity);
	}

	// Verify per-ring-entry independence: SetFrameIndex switches the active entry.
	// After switching to frame 1, shadow count for that entry starts at 0.
	t.SetFrameIndex(1u);
	EXPECT_EQ(t.GetShadowCount(), 0u);

	// Upload to frame 1.
	{
		constexpr uint32_t kN = 3u;
		std::array<ShadowGPU, kN> shadows{};
		for (uint32_t i = 0; i < kN; ++i)
			shadows[i].shadowMapSlot = 100u + i;
		uint32_t written = t.UploadFrame(std::span<const ShadowGPU>(shadows));
		EXPECT_EQ(written, kN);
		EXPECT_EQ(t.GetShadowCount(), kN);
	}

	// Switch back to frame 0: its count is still what was last written there (kCapacity).
	t.SetFrameIndex(0u);
	EXPECT_EQ(t.GetShadowCount(), kCapacity);
}

// shadow_table_observers
TEST(ShadowTable, test_shadow_table_observers)
{
	// Default-constructed: all zero / false.
	{
		ShadowTable t;
		EXPECT_EQ(t.GetBufferAddress(), 0u);
		EXPECT_EQ(t.GetCapacity(), 0u);
		EXPECT_EQ(t.GetShadowCount(), 0u);
		EXPECT_FALSE(t.IsInitialized());
		// IsInitialized ↔ GetBufferAddress != 0
		EXPECT_EQ(t.IsInitialized(), t.GetBufferAddress() != 0u);
	}

	TestContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan device unavailable";
	}

	constexpr uint32_t kCap = 8u;
	constexpr uint32_t kFif = 2u;
	ShadowTable t;
	ASSERT_EQ(t.Initialize(ctx.device, kCap, kFif), RenderError::None);

	// After init: IsInitialized ↔ GetBufferAddress != 0.
	EXPECT_TRUE(t.IsInitialized());
	EXPECT_NE(t.GetBufferAddress(), 0u);
	EXPECT_EQ(t.IsInitialized(), t.GetBufferAddress() != 0u);
	EXPECT_EQ(t.GetCapacity(), kCap);
	EXPECT_EQ(t.GetShadowCount(), 0u);

	// After UploadFrame: GetShadowCount reflects written count.
	constexpr uint32_t kCount = 3u;
	std::array<ShadowGPU, kCount> shadows{};
	t.UploadFrame(std::span<const ShadowGPU>(shadows));
	EXPECT_EQ(t.GetShadowCount(), kCount);

	// Address is stable across UploadFrame calls on the same ring entry.
	VkDeviceAddress addr = t.GetBufferAddress();
	EXPECT_NE(addr, 0u);
	t.UploadFrame(std::span<const ShadowGPU>(shadows));
	EXPECT_EQ(t.GetBufferAddress(), addr);

	// SetFrameIndex changes which ring entry GetBufferAddress / GetShadowCount report.
	t.SetFrameIndex(1u);
	// Frame 1 has never been written: shadow count is 0.
	EXPECT_EQ(t.GetShadowCount(), 0u);
	// Frame 1's address is non-zero (it was created during Initialize).
	VkDeviceAddress addrFrame1 = t.GetBufferAddress();
	EXPECT_NE(addrFrame1, 0u);
	// The two ring entries have distinct addresses.
	EXPECT_NE(addrFrame1, addr);

	// Switch back to frame 0: address and count revert to frame 0's values.
	t.SetFrameIndex(0u);
	EXPECT_EQ(t.GetBufferAddress(), addr);
	EXPECT_EQ(t.GetShadowCount(), kCount);

	// SetFrameIndex on a default-constructed table is safe (updates index only).
	{
		ShadowTable empty;
		empty.SetFrameIndex(0u); // must not crash
		EXPECT_FALSE(empty.IsInitialized());
	}

	// After move: source has all-zero observers.
	ShadowTable moved(std::move(t));
	EXPECT_FALSE(t.IsInitialized());
	EXPECT_EQ(t.GetBufferAddress(), 0u);
	EXPECT_EQ(t.GetCapacity(), 0u);
	EXPECT_EQ(t.GetShadowCount(), 0u);
	EXPECT_EQ(t.IsInitialized(), t.GetBufferAddress() != 0u);

	// Destination has the transferred observers (frame 0 was active at time of move).
	EXPECT_TRUE(moved.IsInitialized());
	EXPECT_EQ(moved.GetBufferAddress(), addr);
	EXPECT_EQ(moved.GetCapacity(), kCap);
	EXPECT_EQ(moved.IsInitialized(), moved.GetBufferAddress() != 0u);
}

// shadow_table_is_not_thread_safe_per_instance
// This semantic documents a design constraint (no internal synchronization)
// rather than a runtime-observable behavior. We verify the documented
// constraint via type-trait and API-shape checks: distinct ShadowTable objects
// are independent (no shared static mutex), and the const observers are
// marked noexcept (a necessary condition for safe concurrent read access).
TEST(ShadowTable, test_shadow_table_is_not_thread_safe_per_instance)
{
	// Const observers must be noexcept — a prerequisite for concurrent reads.
	EXPECT_TRUE(noexcept(std::declval<const ShadowTable&>().GetBufferAddress()));
	EXPECT_TRUE(noexcept(std::declval<const ShadowTable&>().GetCapacity()));
	EXPECT_TRUE(noexcept(std::declval<const ShadowTable&>().GetShadowCount()));
	EXPECT_TRUE(noexcept(std::declval<const ShadowTable&>().IsInitialized()));

	// Two default-constructed ShadowTable objects are independent: default-
	// constructing one does not affect the other's observable state.
	ShadowTable t1;
	ShadowTable t2;
	EXPECT_FALSE(t1.IsInitialized());
	EXPECT_FALSE(t2.IsInitialized());
	EXPECT_EQ(t1.GetBufferAddress(), 0u);
	EXPECT_EQ(t2.GetBufferAddress(), 0u);
}
