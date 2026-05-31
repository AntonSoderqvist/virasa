#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <span>

#include "virasa/core/Logger.h"
#include "virasa/math/Types.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/lighting/LightTable.h"
#include "virasa/renderer/resources/Buffer.h"

using namespace virasa;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
namespace
{

// Build a real Device backed by a real Vulkan instance so that
// LightTable::Initialize can create a real VkBuffer.
// Returns true on success and populates *instance and *device.
bool MakeDevice(Instance& instance, Device& device)
{
	RendererConfig cfg{};
	cfg.applicationName = "LightTableTest";
	cfg.enableValidation = false;
	cfg.requiredInstanceExtensions = nullptr;
	cfg.requiredInstanceExtensionCount = 0;

	if (instance.Initialize(cfg) != RenderError::None)
		return false;

	// No surface — pass VK_NULL_HANDLE. Device selection may fail if the
	// physical device requires a present queue; we accept that in CI.
	if (device.Initialize(instance, VK_NULL_HANDLE) != RenderError::None)
		return false;

	return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// ---- light_type_enum_values_in_declared_order ------------------------------
TEST(LightTable, test_light_type_enum_values_in_declared_order)
{
	// Directional == 0, Point == 1, Spot == 2
	EXPECT_EQ(static_cast<uint32_t>(LightType::Directional), 0u);
	EXPECT_EQ(static_cast<uint32_t>(LightType::Point), 1u);
	EXPECT_EQ(static_cast<uint32_t>(LightType::Spot), 2u);

	// Ordering: Directional < Point < Spot
	EXPECT_LT(
		static_cast<uint32_t>(LightType::Directional), static_cast<uint32_t>(LightType::Point));
	EXPECT_LT(static_cast<uint32_t>(LightType::Point), static_cast<uint32_t>(LightType::Spot));
}

// ---- light_gpu_layout_is_64_bytes_scalar_compatible -----------------------
TEST(LightTable, test_light_gpu_layout_is_64_bytes_scalar_compatible)
{
	// Overall size must be exactly 64 bytes.
	EXPECT_EQ(sizeof(LightGPU), 64u);

	// Byte offsets for each field.
	EXPECT_EQ(offsetof(LightGPU, position), 0u);
	EXPECT_EQ(offsetof(LightGPU, type), 12u);
	EXPECT_EQ(offsetof(LightGPU, direction), 16u);
	EXPECT_EQ(offsetof(LightGPU, range), 28u);
	EXPECT_EQ(offsetof(LightGPU, color), 32u);
	EXPECT_EQ(offsetof(LightGPU, innerConeCos), 44u);
	EXPECT_EQ(offsetof(LightGPU, outerConeCos), 48u);
	EXPECT_EQ(offsetof(LightGPU, shadowIndex), 52u);
	EXPECT_EQ(offsetof(LightGPU, _pad1), 56u);
	EXPECT_EQ(offsetof(LightGPU, _pad2), 60u);

	// Default-constructed values.
	LightGPU g{};
	EXPECT_FLOAT_EQ(g.position.x, 0.0f);
	EXPECT_FLOAT_EQ(g.position.y, 0.0f);
	EXPECT_FLOAT_EQ(g.position.z, 0.0f);
	EXPECT_EQ(g.type, 0u);
	EXPECT_FLOAT_EQ(g.direction.x, 0.0f);
	EXPECT_FLOAT_EQ(g.direction.y, 0.0f);
	EXPECT_FLOAT_EQ(g.direction.z, -1.0f);
	EXPECT_FLOAT_EQ(g.range, 0.0f);
	EXPECT_FLOAT_EQ(g.color.x, 0.0f);
	EXPECT_FLOAT_EQ(g.color.y, 0.0f);
	EXPECT_FLOAT_EQ(g.color.z, 0.0f);
	EXPECT_FLOAT_EQ(g.innerConeCos, 0.0f);
	EXPECT_FLOAT_EQ(g.outerConeCos, 0.0f);
	EXPECT_EQ(g.shadowIndex, -1);
	EXPECT_FLOAT_EQ(g._pad1, 0.0f);
	EXPECT_FLOAT_EQ(g._pad2, 0.0f);

	// LightGPU is copyable and movable.
	LightGPU copy = g;
	EXPECT_EQ(copy.type, g.type);
	LightGPU moved = std::move(copy);
	EXPECT_EQ(moved.type, g.type);
}

// ---- light_table_is_raii_movable_non_copyable ------------------------------
TEST(LightTable, test_light_table_is_raii_movable_non_copyable)
{
	// Not copyable.
	EXPECT_FALSE(std::is_copy_constructible_v<LightTable>);
	EXPECT_FALSE(std::is_copy_assignable_v<LightTable>);

	// Movable.
	EXPECT_TRUE(std::is_move_constructible_v<LightTable>);
	EXPECT_TRUE(std::is_move_assignable_v<LightTable>);

	// Final.
	EXPECT_TRUE(std::is_final_v<LightTable>);

	// Default-constructible.
	EXPECT_TRUE(std::is_default_constructible_v<LightTable>);

	// Move constructor transfers ownership; source becomes default state.
	Instance instance;
	Device device;
	if (!MakeDevice(instance, device))
	{
		GTEST_SKIP() << "Vulkan device unavailable; skipping move-ownership sub-test";
	}

	LightTable tableA;
	ASSERT_EQ(tableA.Initialize(device, 4u), RenderError::None);
	EXPECT_TRUE(tableA.IsInitialized());
	EXPECT_NE(tableA.GetBufferAddress(), 0u);

	LightTable tableB = std::move(tableA);
	EXPECT_TRUE(tableB.IsInitialized());
	EXPECT_NE(tableB.GetBufferAddress(), 0u);

	// Source is now in default state.
	EXPECT_FALSE(tableA.IsInitialized());
	EXPECT_EQ(tableA.GetCapacity(), 0u);
	EXPECT_EQ(tableA.GetLightCount(), 0u);
	EXPECT_EQ(tableA.GetBufferAddress(), 0u);

	// Move assignment: tableA (empty) = std::move(tableB).
	tableA = std::move(tableB);
	EXPECT_TRUE(tableA.IsInitialized());
	EXPECT_FALSE(tableB.IsInitialized());
	EXPECT_EQ(tableB.GetCapacity(), 0u);
	EXPECT_EQ(tableB.GetLightCount(), 0u);
	EXPECT_EQ(tableB.GetBufferAddress(), 0u);

	device.WaitIdle();
}

// ---- light_table_default_constructed_state ---------------------------------
TEST(LightTable, test_light_table_default_constructed_state)
{
	LightTable table;
	EXPECT_FALSE(table.IsInitialized());
	EXPECT_EQ(table.GetCapacity(), 0u);
	EXPECT_EQ(table.GetLightCount(), 0u);
	EXPECT_EQ(table.GetBufferAddress(), static_cast<VkDeviceAddress>(0));
}

// ---- light_table_initialize_creates_host_visible_buffer -------------------
TEST(LightTable, test_light_table_initialize_creates_host_visible_buffer)
{
	Instance instance;
	Device device;
	if (!MakeDevice(instance, device))
	{
		GTEST_SKIP() << "Vulkan device unavailable";
	}

	constexpr uint32_t kMaxLights = 16u;
	LightTable table;

	RenderError err = table.Initialize(device, kMaxLights);
	ASSERT_EQ(err, RenderError::None);

	EXPECT_TRUE(table.IsInitialized());
	EXPECT_EQ(table.GetCapacity(), kMaxLights);
	EXPECT_EQ(table.GetLightCount(), 0u);
	EXPECT_NE(table.GetBufferAddress(), static_cast<VkDeviceAddress>(0));

	// Re-initialization must not leak: call Initialize a second time.
	RenderError err2 = table.Initialize(device, kMaxLights * 2u);
	ASSERT_EQ(err2, RenderError::None);
	EXPECT_TRUE(table.IsInitialized());
	EXPECT_EQ(table.GetCapacity(), kMaxLights * 2u);
	EXPECT_EQ(table.GetLightCount(), 0u);
	EXPECT_NE(table.GetBufferAddress(), static_cast<VkDeviceAddress>(0));

	device.WaitIdle();
}

// ---- light_table_upload_frame_writes_compact_array ------------------------
TEST(LightTable, test_light_table_upload_frame_writes_compact_array)
{
	Instance instance;
	Device device;
	if (!MakeDevice(instance, device))
	{
		GTEST_SKIP() << "Vulkan device unavailable";
	}

	constexpr uint32_t kCapacity = 4u;
	LightTable table;
	ASSERT_EQ(table.Initialize(device, kCapacity), RenderError::None);

	// --- Upload exactly capacity lights ---
	LightGPU lights[kCapacity];
	for (uint32_t i = 0; i < kCapacity; ++i)
	{
		lights[i].type = static_cast<uint32_t>(LightType::Point);
		lights[i].range = static_cast<float>(i + 1);
	}
	uint32_t written = table.UploadFrame(std::span<const LightGPU>(lights, kCapacity));
	EXPECT_EQ(written, kCapacity);
	EXPECT_EQ(table.GetLightCount(), kCapacity);

	// --- Upload fewer than capacity ---
	constexpr uint32_t kFew = 2u;
	LightGPU fewLights[kFew];
	for (uint32_t i = 0; i < kFew; ++i)
		fewLights[i].type = static_cast<uint32_t>(LightType::Directional);
	uint32_t writtenFew = table.UploadFrame(std::span<const LightGPU>(fewLights, kFew));
	EXPECT_EQ(writtenFew, kFew);
	EXPECT_EQ(table.GetLightCount(), kFew);

	// --- Upload more than capacity (overflow clamped, warning logged) ---
	constexpr uint32_t kMany = kCapacity + 3u;
	LightGPU manyLights[kMany];
	for (uint32_t i = 0; i < kMany; ++i)
		manyLights[i].type = static_cast<uint32_t>(LightType::Spot);
	uint32_t writtenMany = table.UploadFrame(std::span<const LightGPU>(manyLights, kMany));
	EXPECT_EQ(writtenMany, kCapacity); // clamped to capacity
	EXPECT_EQ(table.GetLightCount(), kCapacity);

	// --- Upload zero lights ---
	uint32_t writtenZero = table.UploadFrame(std::span<const LightGPU>{});
	EXPECT_EQ(writtenZero, 0u);
	EXPECT_EQ(table.GetLightCount(), 0u);

	device.WaitIdle();
}

// ---- light_table_observers -------------------------------------------------
TEST(LightTable, test_light_table_observers)
{
	// Observers on default-constructed table.
	{
		LightTable table;
		EXPECT_EQ(table.GetBufferAddress(), static_cast<VkDeviceAddress>(0));
		EXPECT_EQ(table.GetCapacity(), 0u);
		EXPECT_EQ(table.GetLightCount(), 0u);
		EXPECT_FALSE(table.IsInitialized());
		// IsInitialized iff GetBufferAddress != 0.
		EXPECT_EQ(table.IsInitialized(), table.GetBufferAddress() != 0u);
	}

	Instance instance;
	Device device;
	if (!MakeDevice(instance, device))
	{
		GTEST_SKIP() << "Vulkan device unavailable";
	}

	constexpr uint32_t kCap = 8u;
	LightTable table;
	ASSERT_EQ(table.Initialize(device, kCap), RenderError::None);

	// After Initialize: capacity, address, count, initialized.
	EXPECT_EQ(table.GetCapacity(), kCap);
	EXPECT_EQ(table.GetLightCount(), 0u);
	EXPECT_TRUE(table.IsInitialized());
	EXPECT_NE(table.GetBufferAddress(), static_cast<VkDeviceAddress>(0));
	// IsInitialized iff GetBufferAddress != 0.
	EXPECT_EQ(table.IsInitialized(), table.GetBufferAddress() != 0u);

	// After UploadFrame: GetLightCount reflects written count.
	LightGPU lights[3]{};
	table.UploadFrame(std::span<const LightGPU>(lights, 3));
	EXPECT_EQ(table.GetLightCount(), 3u);
	EXPECT_EQ(table.GetCapacity(), kCap); // capacity unchanged

	// Address is stable across UploadFrame calls.
	VkDeviceAddress addrBefore = table.GetBufferAddress();
	table.UploadFrame(std::span<const LightGPU>(lights, 1));
	EXPECT_EQ(table.GetBufferAddress(), addrBefore);

	// After move: source observers return default values.
	LightTable moved = std::move(table);
	EXPECT_FALSE(table.IsInitialized());
	EXPECT_EQ(table.GetCapacity(), 0u);
	EXPECT_EQ(table.GetLightCount(), 0u);
	EXPECT_EQ(table.GetBufferAddress(), static_cast<VkDeviceAddress>(0));
	EXPECT_EQ(table.IsInitialized(), table.GetBufferAddress() != 0u);

	device.WaitIdle();
}

// ---- light_table_is_not_thread_safe_per_instance --------------------------
// The contract states that const observers may be called concurrently with
// each other on the same LightTable. We verify this at the type level by
// confirming the observer methods are const-qualified (compile-time check)
// and perform a simple sequential concurrent-read scenario to confirm they
// return consistent values when called back-to-back without mutation.
TEST(LightTable, test_light_table_is_not_thread_safe_per_instance)
{
	// Verify const-qualification of all four observers via pointer-to-member.
	using GetBufferAddressFn = VkDeviceAddress (LightTable::*)() const noexcept;
	using GetCapacityFn = uint32_t (LightTable::*)() const noexcept;
	using GetLightCountFn = uint32_t (LightTable::*)() const noexcept;
	using IsInitializedFn = bool (LightTable::*)() const noexcept;

	GetBufferAddressFn pAddr = &LightTable::GetBufferAddress;
	GetCapacityFn pCap = &LightTable::GetCapacity;
	GetLightCountFn pCount = &LightTable::GetLightCount;
	IsInitializedFn pInit = &LightTable::IsInitialized;

	// Suppress unused-variable warnings.
	(void)pAddr;
	(void)pCap;
	(void)pCount;
	(void)pInit;

	// Sequential read consistency on a default-constructed table.
	const LightTable table;
	EXPECT_EQ(table.GetBufferAddress(), static_cast<VkDeviceAddress>(0));
	EXPECT_EQ(table.GetCapacity(), 0u);
	EXPECT_EQ(table.GetLightCount(), 0u);
	EXPECT_FALSE(table.IsInitialized());
	// All reads are consistent with each other.
	EXPECT_EQ(table.IsInitialized(), table.GetBufferAddress() != 0u);
}
