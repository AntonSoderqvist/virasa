#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

#include "virasa/core/Logger.h"
#include "virasa/math/Types.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/material/Visual.h"
#include "virasa/window/Platform.h"
#include "vulkan/vulkan.h"

using namespace virasa;

namespace
{

// Bring up a real Vulkan instance + device for tests that need one.
// Returns valid=false when the environment cannot support Vulkan.
struct VulkanContext
{
	virasa::window::Platform platform;
	Instance instance;
	Device device;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	bool valid = false;

	VulkanContext()
	{
		Logger::Initialize();

		if (platform.Initialize("VisualTableTest", 1, 1) != ErrorCode::None)
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

		valid = true;
	}

	~VulkanContext()
	{
		if (device.IsInitialized())
			device.WaitIdle();
		platform.Shutdown();
		Logger::Shutdown();
	}

	VulkanContext(const VulkanContext&) = delete;
	VulkanContext& operator=(const VulkanContext&) = delete;
};

} // namespace

TEST(VisualTests, test_alpha_mode_enum_values_in_declared_order)
{
	static_assert(std::is_enum_v<AlphaMode>);
	static_assert(std::is_same_v<std::underlying_type_t<AlphaMode>, uint32_t>);

	EXPECT_EQ(static_cast<uint32_t>(AlphaMode::Opaque), 0u);
	EXPECT_EQ(static_cast<uint32_t>(AlphaMode::Mask), 1u);
	EXPECT_EQ(static_cast<uint32_t>(AlphaMode::Blend), 2u);
	EXPECT_LT(static_cast<uint32_t>(AlphaMode::Opaque), static_cast<uint32_t>(AlphaMode::Mask));
	EXPECT_LT(static_cast<uint32_t>(AlphaMode::Mask), static_cast<uint32_t>(AlphaMode::Blend));
}

TEST(VisualTests, test_material_model_enum_values_in_declared_order)
{
	static_assert(std::is_enum_v<MaterialModel>);
	static_assert(std::is_same_v<std::underlying_type_t<MaterialModel>, uint32_t>);

	EXPECT_EQ(static_cast<uint32_t>(MaterialModel::PBR), 0u);
	EXPECT_EQ(static_cast<uint32_t>(MaterialModel::TerrainHeight), 1u);
	EXPECT_LT(static_cast<uint32_t>(MaterialModel::PBR),
			  static_cast<uint32_t>(MaterialModel::TerrainHeight));
}

TEST(VisualTests, test_pbr_factors_layout_is_64_bytes_scalar_compatible)
{
	static_assert(std::is_default_constructible_v<PBRFactors>);
	static_assert(std::is_copy_constructible_v<PBRFactors>);
	static_assert(std::is_move_constructible_v<PBRFactors>);
	static_assert(std::is_trivially_destructible_v<PBRFactors>);

	EXPECT_EQ(sizeof(PBRFactors), 64u);
	EXPECT_EQ(offsetof(PBRFactors, baseColorFactor), 0u);
	EXPECT_EQ(offsetof(PBRFactors, emissiveFactor), 16u);
	EXPECT_EQ(offsetof(PBRFactors, metallicFactor), 28u);
	EXPECT_EQ(offsetof(PBRFactors, roughnessFactor), 32u);
	EXPECT_EQ(offsetof(PBRFactors, normalScale), 36u);
	EXPECT_EQ(offsetof(PBRFactors, occlusionStrength), 40u);
	EXPECT_EQ(offsetof(PBRFactors, alphaCutoff), 44u);
	EXPECT_EQ(offsetof(PBRFactors, alphaModeBits), 48u);
	EXPECT_EQ(offsetof(PBRFactors, _pad0), 52u);
	EXPECT_EQ(offsetof(PBRFactors, _pad1), 56u);
	EXPECT_EQ(offsetof(PBRFactors, _pad2), 60u);

	const PBRFactors factors;
	EXPECT_FLOAT_EQ(factors.baseColorFactor.x, 0.8f);
	EXPECT_FLOAT_EQ(factors.baseColorFactor.y, 0.8f);
	EXPECT_FLOAT_EQ(factors.baseColorFactor.z, 0.8f);
	EXPECT_FLOAT_EQ(factors.baseColorFactor.w, 1.0f);
	EXPECT_FLOAT_EQ(factors.emissiveFactor.x, 0.0f);
	EXPECT_FLOAT_EQ(factors.emissiveFactor.y, 0.0f);
	EXPECT_FLOAT_EQ(factors.emissiveFactor.z, 0.0f);
	EXPECT_FLOAT_EQ(factors.metallicFactor, 0.0f);
	EXPECT_FLOAT_EQ(factors.roughnessFactor, 1.0f);
	EXPECT_FLOAT_EQ(factors.normalScale, 1.0f);
	EXPECT_FLOAT_EQ(factors.occlusionStrength, 1.0f);
	EXPECT_FLOAT_EQ(factors.alphaCutoff, 0.5f);
	EXPECT_EQ(factors.alphaModeBits, 0u);
	EXPECT_FLOAT_EQ(factors._pad0, 0.0f);
	EXPECT_FLOAT_EQ(factors._pad1, 0.0f);
	EXPECT_FLOAT_EQ(factors._pad2, 0.0f);
}

TEST(VisualTests, test_visual_material_gpu_layout_is_96_bytes_scalar_compatible)
{
	static_assert(std::is_default_constructible_v<VisualMaterialGPU>);
	static_assert(std::is_copy_constructible_v<VisualMaterialGPU>);
	static_assert(std::is_move_constructible_v<VisualMaterialGPU>);
	static_assert(std::is_trivially_destructible_v<VisualMaterialGPU>);

	EXPECT_EQ(sizeof(VisualMaterialGPU), 96u);
	EXPECT_EQ(offsetof(VisualMaterialGPU, factors), 0u);
	EXPECT_EQ(offsetof(VisualMaterialGPU, baseColorTexture), 64u);
	EXPECT_EQ(offsetof(VisualMaterialGPU, normalTexture), 68u);
	EXPECT_EQ(offsetof(VisualMaterialGPU, metallicRoughnessTexture), 72u);
	EXPECT_EQ(offsetof(VisualMaterialGPU, occlusionTexture), 76u);
	EXPECT_EQ(offsetof(VisualMaterialGPU, emissiveTexture), 80u);
	EXPECT_EQ(offsetof(VisualMaterialGPU, _pad3), 84u);
	EXPECT_EQ(offsetof(VisualMaterialGPU, _pad4), 88u);
	EXPECT_EQ(offsetof(VisualMaterialGPU, _pad5), 92u);

	const VisualMaterialGPU materialGpu;
	EXPECT_EQ(materialGpu.baseColorTexture, 0u);
	EXPECT_EQ(materialGpu.normalTexture, 0u);
	EXPECT_EQ(materialGpu.metallicRoughnessTexture, 0u);
	EXPECT_EQ(materialGpu.occlusionTexture, 0u);
	EXPECT_EQ(materialGpu.emissiveTexture, 0u);
	EXPECT_EQ(materialGpu._pad3, 0u);
	EXPECT_EQ(materialGpu._pad4, 0u);
	EXPECT_EQ(materialGpu._pad5, 0u);
}

TEST(VisualTests, test_visual_material_describes_cpu_authoring_state)
{
	static_assert(std::is_default_constructible_v<VisualMaterial>);
	static_assert(std::is_copy_constructible_v<VisualMaterial>);
	static_assert(std::is_move_constructible_v<VisualMaterial>);
	static_assert(std::is_trivially_destructible_v<VisualMaterial>);

	EXPECT_EQ(offsetof(VisualMaterial, factors), 0u);
	EXPECT_EQ(offsetof(VisualMaterial, baseColorTexture), 64u);
	EXPECT_EQ(offsetof(VisualMaterial, normalTexture), 68u);
	EXPECT_EQ(offsetof(VisualMaterial, metallicRoughnessTexture), 72u);
	EXPECT_EQ(offsetof(VisualMaterial, occlusionTexture), 76u);
	EXPECT_EQ(offsetof(VisualMaterial, emissiveTexture), 80u);
	EXPECT_EQ(offsetof(VisualMaterial, alphaMode), 84u);
	EXPECT_EQ(offsetof(VisualMaterial, doubleSided), 88u);

	const VisualMaterial material;
	EXPECT_FLOAT_EQ(material.factors.baseColorFactor.x, 0.8f);
	EXPECT_FLOAT_EQ(material.factors.baseColorFactor.y, 0.8f);
	EXPECT_FLOAT_EQ(material.factors.baseColorFactor.z, 0.8f);
	EXPECT_FLOAT_EQ(material.factors.baseColorFactor.w, 1.0f);
	EXPECT_FLOAT_EQ(material.factors.emissiveFactor.x, 0.0f);
	EXPECT_FLOAT_EQ(material.factors.emissiveFactor.y, 0.0f);
	EXPECT_FLOAT_EQ(material.factors.emissiveFactor.z, 0.0f);
	EXPECT_FLOAT_EQ(material.factors.metallicFactor, 0.0f);
	EXPECT_FLOAT_EQ(material.factors.roughnessFactor, 1.0f);
	EXPECT_FLOAT_EQ(material.factors.normalScale, 1.0f);
	EXPECT_FLOAT_EQ(material.factors.occlusionStrength, 1.0f);
	EXPECT_FLOAT_EQ(material.factors.alphaCutoff, 0.5f);
	EXPECT_EQ(material.baseColorTexture, 0u);
	EXPECT_EQ(material.normalTexture, 0u);
	EXPECT_EQ(material.metallicRoughnessTexture, 0u);
	EXPECT_EQ(material.occlusionTexture, 0u);
	EXPECT_EQ(material.emissiveTexture, 0u);
	EXPECT_EQ(material.alphaMode, AlphaMode::Opaque);
	EXPECT_FALSE(material.doubleSided);
	EXPECT_EQ(material.materialModel, MaterialModel::PBR);
}

TEST(VisualTests, test_visual_material_raster_state_holds_pipeline_keys)
{
	static_assert(std::is_default_constructible_v<VisualMaterialRasterState>);
	static_assert(std::is_copy_constructible_v<VisualMaterialRasterState>);
	static_assert(std::is_move_constructible_v<VisualMaterialRasterState>);
	static_assert(std::is_trivially_destructible_v<VisualMaterialRasterState>);

	EXPECT_EQ(offsetof(VisualMaterialRasterState, alphaMode), 0u);
	EXPECT_EQ(offsetof(VisualMaterialRasterState, doubleSided), 4u);

	const VisualMaterialRasterState defaultState;
	EXPECT_EQ(defaultState.alphaMode, AlphaMode::Opaque);
	EXPECT_FALSE(defaultState.doubleSided);
	EXPECT_EQ(defaultState.materialModel, MaterialModel::PBR);

	const VisualMaterialRasterState sameState{};
	EXPECT_EQ(defaultState.alphaMode, sameState.alphaMode);
	EXPECT_EQ(defaultState.doubleSided, sameState.doubleSided);
	EXPECT_EQ(defaultState.materialModel, sameState.materialModel);

	VisualMaterialRasterState differentState;
	differentState.alphaMode = AlphaMode::Blend;
	differentState.doubleSided = true;
	differentState.materialModel = MaterialModel::TerrainHeight;
	EXPECT_NE(defaultState.alphaMode, differentState.alphaMode);
	EXPECT_NE(defaultState.doubleSided, differentState.doubleSided);
	EXPECT_NE(defaultState.materialModel, differentState.materialModel);
}

TEST(VisualTests, test_visual_material_table_is_raii_movable_non_copyable)
{
	static_assert(std::is_default_constructible_v<VisualMaterialTable>);
	static_assert(!std::is_copy_constructible_v<VisualMaterialTable>);
	static_assert(!std::is_copy_assignable_v<VisualMaterialTable>);
	static_assert(std::is_move_constructible_v<VisualMaterialTable>);
	static_assert(std::is_move_assignable_v<VisualMaterialTable>);
	static_assert(std::is_final_v<VisualMaterialTable>);

	VisualMaterialTable table;
	EXPECT_FALSE(table.IsInitialized());
	EXPECT_EQ(table.GetMaxMaterials(), 0u);
	EXPECT_EQ(table.GetBufferAddress(), 0u);

	VisualMaterialTable movedConstructed(std::move(table));
	EXPECT_FALSE(table.IsInitialized());
	EXPECT_EQ(table.GetMaxMaterials(), 0u);
	EXPECT_EQ(table.GetBufferAddress(), 0u);
	EXPECT_FALSE(movedConstructed.IsInitialized());
	EXPECT_EQ(movedConstructed.GetMaxMaterials(), 0u);
	EXPECT_EQ(movedConstructed.GetBufferAddress(), 0u);

	VisualMaterialTable destination;
	destination = std::move(movedConstructed);
	EXPECT_FALSE(movedConstructed.IsInitialized());
	EXPECT_EQ(movedConstructed.GetMaxMaterials(), 0u);
	EXPECT_EQ(movedConstructed.GetBufferAddress(), 0u);
	EXPECT_FALSE(destination.IsInitialized());
	EXPECT_EQ(destination.GetMaxMaterials(), 0u);
	EXPECT_EQ(destination.GetBufferAddress(), 0u);
}

TEST(VisualTests, test_visual_material_table_default_constructed_state)
{
	const VisualMaterialTable table;
	EXPECT_FALSE(table.IsInitialized());
	EXPECT_EQ(table.GetMaxMaterials(), 0u);
	EXPECT_EQ(table.GetBufferAddress(), 0u);
}

TEST(VisualTests, test_visual_material_table_initialize_creates_host_visible_buffer)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	VisualMaterialTable table;
	constexpr uint32_t kMax = 16u;
	ASSERT_EQ(table.Initialize(ctx.device, kMax), RenderError::None);

	EXPECT_TRUE(table.IsInitialized());
	EXPECT_EQ(table.GetMaxMaterials(), kMax);
	EXPECT_NE(table.GetBufferAddress(), 0u);

	// Re-initialization tears down and rebuilds.
	ASSERT_EQ(table.Initialize(ctx.device, kMax * 2u), RenderError::None);
	EXPECT_TRUE(table.IsInitialized());
	EXPECT_EQ(table.GetMaxMaterials(), kMax * 2u);
	EXPECT_NE(table.GetBufferAddress(), 0u);

	ctx.device.WaitIdle();
}

TEST(VisualTests, test_visual_material_table_allocate_claims_id_and_writes_record)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	VisualMaterialTable table;
	constexpr uint32_t kMax = 4u;
	ASSERT_EQ(table.Initialize(ctx.device, kMax), RenderError::None);

	VisualMaterial mat;
	mat.alphaMode = AlphaMode::Mask;
	mat.doubleSided = true;
	mat.materialModel = MaterialModel::TerrainHeight;
	mat.factors.baseColorFactor = math::Vec4(0.25f, 0.5f, 0.75f, 1.0f);
	mat.baseColorTexture = 7u;

	uint32_t id0 = table.Allocate(mat);
	EXPECT_NE(id0, UINT32_MAX);
	EXPECT_LT(id0, kMax);

	VisualMaterialRasterState rs = table.GetRasterState(id0);
	EXPECT_EQ(rs.alphaMode, AlphaMode::Mask);
	EXPECT_TRUE(rs.doubleSided);
	EXPECT_EQ(rs.materialModel, MaterialModel::TerrainHeight);

	// Allocate remaining slots, then exhaust.
	uint32_t id1 = table.Allocate(VisualMaterial{});
	uint32_t id2 = table.Allocate(VisualMaterial{});
	uint32_t id3 = table.Allocate(VisualMaterial{});
	EXPECT_NE(id1, UINT32_MAX);
	EXPECT_NE(id2, UINT32_MAX);
	EXPECT_NE(id3, UINT32_MAX);
	EXPECT_NE(id1, id0);
	EXPECT_NE(id2, id0);
	EXPECT_NE(id3, id0);

	uint32_t overflow = table.Allocate(VisualMaterial{});
	EXPECT_EQ(overflow, UINT32_MAX);

	ctx.device.WaitIdle();
}

TEST(VisualTests, test_visual_material_table_update_rewrites_record_and_raster)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	VisualMaterialTable table;
	ASSERT_EQ(table.Initialize(ctx.device, 8u), RenderError::None);

	VisualMaterial original;
	uint32_t id = table.Allocate(original);
	ASSERT_NE(id, UINT32_MAX);

	VisualMaterialRasterState rs0 = table.GetRasterState(id);
	EXPECT_EQ(rs0.alphaMode, AlphaMode::Opaque);
	EXPECT_FALSE(rs0.doubleSided);
	EXPECT_EQ(rs0.materialModel, MaterialModel::PBR);

	VisualMaterial updated;
	updated.alphaMode = AlphaMode::Blend;
	updated.doubleSided = true;
	updated.materialModel = MaterialModel::TerrainHeight;
	EXPECT_EQ(table.Update(id, updated), RenderError::None);

	VisualMaterialRasterState rs1 = table.GetRasterState(id);
	EXPECT_EQ(rs1.alphaMode, AlphaMode::Blend);
	EXPECT_TRUE(rs1.doubleSided);
	EXPECT_EQ(rs1.materialModel, MaterialModel::TerrainHeight);

	ctx.device.WaitIdle();
}

TEST(VisualTests, test_visual_material_table_free_returns_id_to_freelist)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	VisualMaterialTable table;
	constexpr uint32_t kMax = 2u;
	ASSERT_EQ(table.Initialize(ctx.device, kMax), RenderError::None);

	uint32_t id0 = table.Allocate(VisualMaterial{});
	uint32_t id1 = table.Allocate(VisualMaterial{});
	ASSERT_NE(id0, UINT32_MAX);
	ASSERT_NE(id1, UINT32_MAX);

	// Table is full now.
	EXPECT_EQ(table.Allocate(VisualMaterial{}), UINT32_MAX);

	// Free one id; the next Allocate should reuse it (stack-like freelist).
	table.Free(id0);
	uint32_t reused = table.Allocate(VisualMaterial{});
	EXPECT_EQ(reused, id0);

	ctx.device.WaitIdle();
}

TEST(VisualTests, test_visual_material_table_observers)
{
	const VisualMaterialTable table;
	EXPECT_FALSE(table.IsInitialized());
	EXPECT_EQ(table.GetBufferAddress(), 0u);
	EXPECT_EQ(table.GetMaxMaterials(), 0u);
	EXPECT_EQ(table.IsInitialized(), table.GetBufferAddress() != 0u);

	// A default-constructed table has no allocated slots, so IsAllocated is
	// false for every id including the invalid sentinel and never reads out
	// of bounds.
	EXPECT_FALSE(table.IsAllocated(0u));
	EXPECT_FALSE(table.IsAllocated(0xFFFFFFFFu));

	// With an initialized table, IsAllocated tracks allocation and freeing.
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	VisualMaterialTable allocated;
	ASSERT_EQ(allocated.Initialize(ctx.device, 4u), RenderError::None);

	uint32_t id = allocated.Allocate(VisualMaterial{});
	ASSERT_NE(id, UINT32_MAX);
	EXPECT_TRUE(allocated.IsAllocated(id));
	EXPECT_FALSE(allocated.IsAllocated(0xFFFFFFFFu));
	EXPECT_FALSE(allocated.IsAllocated(allocated.GetMaxMaterials()));

	allocated.Free(id);
	EXPECT_FALSE(allocated.IsAllocated(id));
}

TEST(VisualTests, test_visual_material_table_is_not_thread_safe_per_instance)
{
	SUCCEED();
}
