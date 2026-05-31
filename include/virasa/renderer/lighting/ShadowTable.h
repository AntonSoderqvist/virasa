#ifndef VIRASA_RENDERER_LIGHTING_SHADOW_TABLE_H
#define VIRASA_RENDERER_LIGHTING_SHADOW_TABLE_H

#include <cstdint>
#include <span>
#include <vector>

#include "virasa/math/Types.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/resources/Buffer.h"
#include "vulkan/vulkan.h"

namespace virasa
{

/**
 * @brief GPU-side record for a single shadow-casting light in a frame.
 *
 * ShadowGPU is the shader-side record written into the shadow buffer.
 * It is the companion record referenced by LightGPU::shadowIndex.
 * The struct is exactly 80 bytes, scalar-layout compatible.
 */
struct ShadowGPU
{
public:
	/// Light-space view-projection matrix (column-major, 64 bytes).
	virasa::math::Mat4 lightViewProj = virasa::math::Mat4(1.0f);
	/// Bindless texture-array slot of the shadow depth map.
	uint32_t shadowMapSlot = 0u;
	/// Constant depth bias to combat shadow acne.
	float depthBias = 0.0015f;
	/// Slope-scaled depth bias.
	float slopeBias = 0.0025f;
	/// Padding to round struct to 80-byte boundary.
	uint32_t _pad0 = 0u;
};

static_assert(sizeof(ShadowGPU) == 80, "ShadowGPU must be exactly 80 bytes");
static_assert(offsetof(ShadowGPU, lightViewProj) == 0, "lightViewProj must be at offset 0");
static_assert(offsetof(ShadowGPU, shadowMapSlot) == 64, "shadowMapSlot must be at offset 64");
static_assert(offsetof(ShadowGPU, depthBias) == 68, "depthBias must be at offset 68");
static_assert(offsetof(ShadowGPU, slopeBias) == 72, "slopeBias must be at offset 72");
static_assert(offsetof(ShadowGPU, _pad0) == 76, "_pad0 must be at offset 76");

/**
 * @brief Manages a GPU-resident buffer of ShadowGPU records for one frame.
 *
 * ShadowTable owns a host-visible, host-coherent storage buffer sized for
 * up to max_shadows shadow records. Each frame, UploadFrame writes the
 * current shadow list into the mapped buffer. The buffer's device address
 * is suitable for use as a push constant and shader buffer_reference.
 *
 * ShadowTable is final, non-copyable, and movable.
 */
class ShadowTable final
{
public:
	/**
	 * @brief Default-constructs a ShadowTable that owns no Vulkan resources.
	 */
	ShadowTable() = default;

	~ShadowTable();

	ShadowTable(const ShadowTable&) = delete;
	ShadowTable& operator=(const ShadowTable&) = delete;

	ShadowTable(ShadowTable&& other) noexcept;
	ShadowTable& operator=(ShadowTable&& other) noexcept;

	/**
	 * @brief Allocates the GPU shadow buffer ring and maps each entry persistently.
	 * @param device An initialized Device to allocate from.
	 * @param max_shadows Maximum number of shadow records each buffer can hold.
	 * @param frames_in_flight Number of ring entries (one per in-flight frame).
	 * @return RenderError::None on success, or a specific error code.
	 */
	[[nodiscard]] RenderError Initialize(
		const Device& device,
		uint32_t max_shadows,
		uint32_t frames_in_flight);

	/**
	 * @brief Selects the current ring entry for subsequent UploadFrame / observer calls.
	 * @param frame_index Index in [0, frames_in_flight).
	 */
	void SetFrameIndex(uint32_t frame_index);

	/**
	 * @brief Writes shadow records into the current-frame ring entry's mapped buffer.
	 * @param shadows Span of ShadowGPU records to upload (clamped to capacity).
	 * @return The number of records actually written.
	 */
	uint32_t UploadFrame(std::span<const ShadowGPU> shadows);

	/**
	 * @brief Returns the VkDeviceAddress of the current-frame ring entry's shadow buffer.
	 * @return Device address, or 0 if not initialized.
	 */
	[[nodiscard]] VkDeviceAddress GetBufferAddress() const noexcept;

	/**
	 * @brief Returns the maximum number of shadow records each buffer can hold.
	 * @return Capacity, or 0 if not initialized.
	 */
	[[nodiscard]] uint32_t GetCapacity() const noexcept;

	/**
	 * @brief Returns the number of records written by the most recent UploadFrame
	 *        targeting the current-frame ring entry.
	 * @return Shadow count, or 0 if no UploadFrame has targeted this entry yet.
	 */
	[[nodiscard]] uint32_t GetShadowCount() const noexcept;

	/**
	 * @brief Returns true if this ShadowTable owns an initialized Buffer ring.
	 * @return True if initialized.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

private:
	/// Per-frame ring entries.
	std::vector<Buffer> _buffers;
	/// Persistently-mapped pointers, one per ring entry.
	std::vector<void*> _mappedPtrs;
	/// Cached device addresses, one per ring entry.
	std::vector<VkDeviceAddress> _bufferAddresses;
	/// Cached shadow counts, one per ring entry.
	std::vector<uint32_t> _shadowCounts;
	/// Maximum shadow records per buffer.
	uint32_t _capacity = 0;
	/// Number of ring entries.
	uint32_t _framesInFlight = 0;
	/// Currently selected ring entry.
	uint32_t _currentFrame = 0;
};

} // namespace virasa

#endif // VIRASA_RENDERER_LIGHTING_SHADOW_TABLE_H
