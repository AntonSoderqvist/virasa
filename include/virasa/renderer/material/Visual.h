#ifndef VIRASA_RENDERER_MATERIAL_VISUAL_H
#define VIRASA_RENDERER_MATERIAL_VISUAL_H

#include <cstdint>
#include <vector>

#include "virasa/math/Types.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/resources/Buffer.h"
#include "vulkan/vulkan.h"

namespace virasa
{

/**
 * @brief Alpha blending mode for a visual material surface.
 *
 * Opaque disables blending and writes to the depth buffer.
 * Mask performs alpha-test discard against alphaCutoff and writes to the depth buffer.
 * Blend performs source-over alpha blending with depth writes disabled.
 */
enum class AlphaMode : uint32_t
{
	Opaque = 0,
	Mask,
	Blend
};

/**
 * @brief Scalar PBR material parameters matching the 64-byte GPU layout.
 *
 * Members are laid out to match the GLSL scalar-layout buffer_reference block
 * used by the fragment shader. The struct is exactly 64 bytes.
 */
struct PBRFactors
{
	public:
	virasa::math::Vec4 baseColorFactor = virasa::math::Vec4(0.8f, 0.8f, 0.8f, 1.0f);
	virasa::math::Vec3 emissiveFactor = virasa::math::Vec3(0.0f);
	float metallicFactor = 0.0f;
	float roughnessFactor = 1.0f;
	float normalScale = 1.0f;
	float occlusionStrength = 1.0f;
	float alphaCutoff = 0.5f;
	uint32_t alphaModeBits = 0u;
	float _pad0 = 0.0f;
	float _pad1 = 0.0f;
	float _pad2 = 0.0f;
};

static_assert(sizeof(PBRFactors) == 64, "PBRFactors must be exactly 64 bytes");

/**
 * @brief GPU-side material record written into the material buffer.
 *
 * Exactly 96 bytes. Shaders must use scalar layout qualifier to match this layout.
 * Callers do not normally construct this directly; VisualMaterialTable builds it.
 */
struct VisualMaterialGPU
{
	public:
	PBRFactors factors;
	uint32_t baseColorTexture = 0u;
	uint32_t normalTexture = 0u;
	uint32_t metallicRoughnessTexture = 0u;
	uint32_t occlusionTexture = 0u;
	uint32_t emissiveTexture = 0u;
	uint32_t _pad3 = 0u;
	uint32_t _pad4 = 0u;
	uint32_t _pad5 = 0u;
};

static_assert(sizeof(VisualMaterialGPU) == 96, "VisualMaterialGPU must be exactly 96 bytes");

/**
 * @brief CPU-side authoring representation of a visual surface description.
 *
 * A default-constructed VisualMaterial describes a fully-rough non-metallic opaque
 * surface with light grey base color and all texture indices pointing at bindless slot 0.
 */
struct VisualMaterial
{
	public:
	PBRFactors factors;
	uint32_t baseColorTexture = 0u;
	uint32_t normalTexture = 0u;
	uint32_t metallicRoughnessTexture = 0u;
	uint32_t occlusionTexture = 0u;
	uint32_t emissiveTexture = 0u;
	AlphaMode alphaMode = AlphaMode::Opaque;
	bool doubleSided = false;
};

/**
 * @brief Subset of VisualMaterial fields that drive pipeline state selection.
 *
 * Used as a bucketing key when sorting draws into pipeline groups.
 * alphaMode selects among opaque/mask/blend pipeline permutations.
 * doubleSided selects between cull-back and cull-none pipeline permutations.
 */
struct VisualMaterialRasterState
{
	public:
	AlphaMode alphaMode = AlphaMode::Opaque;
	bool doubleSided = false;
};

/**
 * @brief Owns the GPU material buffer and manages allocation of material ids.
 *
 * Maintains a host-visible, host-coherent buffer of VisualMaterialGPU records
 * indexed by material id. Provides a free-slot list for id management and a
 * CPU-side raster-state vector for pipeline bucketing.
 *
 * Not copyable. Movable. Not thread-safe per instance.
 */
class VisualMaterialTable final
{
	public:
	/// @brief Default constructor. Owns no resources; IsInitialized returns false.
	VisualMaterialTable() = default;

	VisualMaterialTable(const VisualMaterialTable&) = delete;
	VisualMaterialTable& operator=(const VisualMaterialTable&) = delete;

	/// @brief Move constructor. Transfers ownership from source; source is left
	/// default-constructed.
	VisualMaterialTable(VisualMaterialTable&& other) noexcept;

	/// @brief Move assignment. Tears down existing resources, then transfers from source.
	VisualMaterialTable& operator=(VisualMaterialTable&& other) noexcept;

	~VisualMaterialTable();

	/**
	 * @brief Create the GPU material buffer and initialize the free-slot list.
	 * @param device An initialized Device to create the buffer against.
	 * @param max_materials Maximum number of materials the table can hold (must be > 0).
	 * @return RenderError::None on success, or a specific error code on failure.
	 */
	[[nodiscard]] RenderError Initialize(const Device& device, uint32_t max_materials);

	/**
	 * @brief Claim a free id, write the GPU record, and return the id.
	 * @param material The CPU-side material description to allocate.
	 * @return The allocated material id, or UINT32_MAX if no free slots remain.
	 */
	[[nodiscard]] uint32_t Allocate(const VisualMaterial& material);

	/**
	 * @brief Rewrite the GPU record and raster state for an already-allocated id.
	 * @param id A valid allocated material id.
	 * @param material The new material description.
	 * @return RenderError::None on success.
	 */
	RenderError Update(uint32_t id, const VisualMaterial& material);

	/**
	 * @brief Return id to the free-slot list. Does not zero the GPU record.
	 * @param id A valid allocated material id.
	 */
	void Free(uint32_t id);

	/**
	 * @brief Return the raster state for an allocated material id.
	 * @param id A valid allocated material id.
	 * @return The VisualMaterialRasterState for that id.
	 */
	[[nodiscard]] VisualMaterialRasterState GetRasterState(uint32_t id) const noexcept;

	/**
	 * @brief Return true if id names a currently-allocated material slot.
	 * @param id Any material id, including the invalid sentinel 0xFFFFFFFFu.
	 * @return true if id is in range and not currently freed.
	 */
	[[nodiscard]] bool IsAllocated(uint32_t id) const noexcept;

	/**
	 * @brief Return the VkDeviceAddress of the material buffer.
	 * @return The device address, or 0 if not initialized.
	 */
	[[nodiscard]] VkDeviceAddress GetBufferAddress() const noexcept;

	/**
	 * @brief Return the max_materials value supplied to Initialize, or 0 if not initialized.
	 * @return Maximum material count.
	 */
	[[nodiscard]] uint32_t GetMaxMaterials() const noexcept;

	/**
	 * @brief Return true if this table currently owns an initialized Buffer.
	 * @return Initialization state.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

	private:
	Buffer _buffer;
	void* _mapped = nullptr;
	VkDeviceAddress _bufferAddress = 0;
	uint32_t _maxMaterials = 0;
	std::vector<VisualMaterialRasterState> _rasterStates;
	std::vector<uint32_t> _freeSlots;

	void Teardown();
	static VisualMaterialGPU BuildGPURecord(const VisualMaterial& material);
};

} // namespace virasa

#endif // VIRASA_RENDERER_MATERIAL_VISUAL_H
