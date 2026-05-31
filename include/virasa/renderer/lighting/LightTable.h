#ifndef VIRASA_RENDERER_LIGHTING_LIGHTTABLE_H
#define VIRASA_RENDERER_LIGHTING_LIGHTTABLE_H

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
 * @brief Identifies the type of a light source for shader branching.
 *
 * The underlying uint32_t type mirrors directly into LightGPU::type so
 * shaders can branch without sign-extension or repacking.
 */
enum class LightType : uint32_t
{
	Directional = 0,
	Point = 1,
	Spot = 2,
};

/**
 * @brief Shader-side record for one active light, tightly packed to 64 bytes.
 *
 * The layout matches the GLSL buffer_reference block that uses scalar layout
 * (GL_EXT_scalar_block_layout). Do not use std430 layout in shaders — it
 * inserts padding after each vec3 and produces a non-matching stride.
 */
struct LightGPU
{
	public:
	/// World-space position (meaningful for Point and Spot).
	virasa::math::Vec3 position = virasa::math::Vec3(0.0f);
	/// Light type as uint32_t; equals static_cast<uint32_t>(LightType::X).
	uint32_t type = 0u;
	/// World-space direction / cone axis (meaningful for Directional and Spot).
	virasa::math::Vec3 direction = virasa::math::Vec3(0.0f, 0.0f, -1.0f);
	/// World-space radius beyond which contribution is zero (Point, Spot).
	float range = 0.0f;
	/// Linear-space RGB color pre-multiplied by intensity.
	virasa::math::Vec3 color = virasa::math::Vec3(0.0f);
	/// Cosine of the inner cone half-angle (Spot only).
	float innerConeCos = 0.0f;
	/// Cosine of the outer cone half-angle (Spot only).
	float outerConeCos = 0.0f;
	/// Index into the shadow-record buffer; -1 means no shadow.
	int32_t shadowIndex = -1;
	/// Padding to reach 64-byte boundary — must not be read by shaders.
	float _pad1 = 0.0f;
	float _pad2 = 0.0f;
};

static_assert(sizeof(LightGPU) == 64,
	"LightGPU must be exactly 64 bytes for scalar-layout shader compatibility.");

/**
 * @brief Manages a host-visible, persistently-mapped GPU buffer of LightGPU records.
 *
 * LightTable owns one virasa::renderer::resources::Buffer sized for up to
 * max_lights records. Each frame the caller rebuilds the entire light list via
 * UploadFrame; the resulting buffer address is passed to the fragment shader as
 * a push constant and dereferenced through a buffer_reference (scalar layout).
 *
 * LightTable is not copyable. It is movable; after a move the source is left in
 * the same observable state as a default-constructed LightTable.
 */
class LightTable final
{
	public:
	/**
	 * @brief Default-constructs a LightTable that owns no Vulkan resources.
	 *
	 * IsInitialized returns false, GetCapacity returns 0, GetLightCount returns 0,
	 * and GetBufferAddress returns 0 until Initialize is called.
	 */
	LightTable() = default;

	/// Not copyable.
	LightTable(const LightTable&) = delete;
	LightTable& operator=(const LightTable&) = delete;

	/**
	 * @brief Move-constructs, transferring ownership from @p other.
	 *
	 * After the move @p other is left in the default-constructed state.
	 */
	LightTable(LightTable&& other) noexcept;

	/**
	 * @brief Move-assigns, tearing down any existing resources before taking
	 *        ownership from @p other.
	 *
	 * After the move @p other is left in the default-constructed state.
	 */
	LightTable& operator=(LightTable&& other) noexcept;

	/**
	 * @brief Destroys the LightTable, releasing the owned Buffer ring.
	 *
	 * Destroying a default-constructed or moved-from LightTable is well-defined
	 * and performs no Vulkan calls.
	 */
	~LightTable() = default;

	/**
	 * @brief Creates a ring of GPU light buffers, one per frame in flight.
	 *
	 * If this LightTable already owns resources, they are torn down first.
	 * Each buffer is host-visible and host-coherent so UploadFrame can write
	 * directly into mapped memory without explicit flush or staging.
	 *
	 * @param device           An initialized Device that must outlive this LightTable.
	 * @param max_lights       Maximum number of LightGPU records per buffer; must be > 0.
	 * @param frames_in_flight Number of ring entries; must be > 0.
	 * @return RenderError::None on success, or a propagated error code.
	 */
	[[nodiscard]] RenderError Initialize(
		const Device& device,
		uint32_t max_lights,
		uint32_t frames_in_flight);

	/**
	 * @brief Selects the current ring entry for subsequent UploadFrame / observer calls.
	 *
	 * @param frame_index Must be in [0, frames_in_flight).
	 */
	void SetFrameIndex(uint32_t frame_index);

	/**
	 * @brief Writes the supplied lights into the current ring entry's mapped buffer
	 *        and returns the number of records written.
	 *
	 * Must only be called on an initialized LightTable. If the span exceeds
	 * capacity, excess lights are dropped and a warning is logged.
	 *
	 * @param lights Per-frame light records to upload.
	 * @return Number of records written (min(lights.size(), GetCapacity())).
	 */
	uint32_t UploadFrame(std::span<const LightGPU> lights);

	/**
	 * @brief Returns the VkDeviceAddress of the current ring entry's light buffer.
	 *
	 * Suitable for passing into a push constant and dereferencing in the
	 * fragment shader as a buffer_reference (scalar layout). Returns 0 if
	 * not initialized.
	 *
	 * @return GPU device address, or 0 if not initialized.
	 */
	[[nodiscard]] VkDeviceAddress GetBufferAddress() const noexcept;

	/**
	 * @brief Returns the max_lights value supplied to Initialize, or 0 if not
	 *        initialized.
	 *
	 * @return Light buffer capacity in records.
	 */
	[[nodiscard]] uint32_t GetCapacity() const noexcept;

	/**
	 * @brief Returns the number of records written by the most recent UploadFrame
	 *        call targeting the current ring entry, or 0 if none or not initialized.
	 *
	 * @return Count of valid light records in the current ring entry's buffer.
	 */
	[[nodiscard]] uint32_t GetLightCount() const noexcept;

	/**
	 * @brief Returns true if this LightTable currently owns its Buffer ring as the
	 *        result of a successful Initialize call.
	 *
	 * Equivalent to (GetBufferAddress() != 0).
	 *
	 * @return true if initialized.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

	private:
	/// Per-frame-in-flight GPU-resident light record buffers.
	std::vector<Buffer> _buffers;
	/// Persistently-mapped host pointers, one per ring entry.
	std::vector<void*> _mappedPtrs;
	/// Cached VkDeviceAddress per ring entry.
	std::vector<VkDeviceAddress> _bufferAddresses;
	/// Cached light count per ring entry (written by UploadFrame).
	std::vector<uint32_t> _lightCounts;
	/// Cached max_lights from the last successful Initialize call.
	uint32_t _capacity = 0;
	/// Number of ring entries.
	uint32_t _framesInFlight = 0;
	/// Current ring entry index (set via SetFrameIndex).
	uint32_t _currentFrame = 0;
};

} // namespace virasa

#endif // VIRASA_RENDERER_LIGHTING_LIGHTTABLE_H
