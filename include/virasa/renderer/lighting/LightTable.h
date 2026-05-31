#ifndef VIRASA_RENDERER_LIGHTING_LIGHTTABLE_H
#define VIRASA_RENDERER_LIGHTING_LIGHTTABLE_H

#include <cstdint>
#include <span>

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
	 * @brief Destroys the LightTable, releasing the owned Buffer.
	 *
	 * Destroying a default-constructed or moved-from LightTable is well-defined
	 * and performs no Vulkan calls.
	 */
	~LightTable() = default;

	/**
	 * @brief Creates the GPU light buffer sized for @p max_lights records.
	 *
	 * If this LightTable already owns resources, they are torn down first.
	 * The buffer is host-visible and host-coherent so UploadFrame can write
	 * directly into mapped memory without explicit flush or staging.
	 *
	 * @param device     An initialized Device that must outlive this LightTable.
	 * @param max_lights Maximum number of LightGPU records; must be > 0.
	 * @return RenderError::None on success, or a propagated error code.
	 */
	[[nodiscard]] RenderError Initialize(const Device& device, uint32_t max_lights);

	/**
	 * @brief Writes the supplied lights into the mapped buffer and returns the
	 *        number of records written.
	 *
	 * Must only be called on an initialized LightTable. If the span exceeds
	 * capacity, excess lights are dropped and a warning is logged.
	 *
	 * @param lights Per-frame light records to upload.
	 * @return Number of records written (min(lights.size(), GetCapacity())).
	 */
	uint32_t UploadFrame(std::span<const LightGPU> lights);

	/**
	 * @brief Returns the VkDeviceAddress of the owned light buffer.
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
	 * @brief Returns the number of records written by the most recent
	 *        UploadFrame call, or 0 if no upload has occurred or the table is
	 *        not initialized.
	 *
	 * @return Count of valid light records in the buffer.
	 */
	[[nodiscard]] uint32_t GetLightCount() const noexcept;

	/**
	 * @brief Returns true if this LightTable currently owns a Buffer as the
	 *        result of a successful Initialize call.
	 *
	 * Equivalent to (GetBufferAddress() != 0).
	 *
	 * @return true if initialized.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

	private:
	/// The GPU-resident light record buffer.
	Buffer _buffer;
	/// Persistently-mapped host pointer into _buffer's backing memory.
	void* _mapped = nullptr;
	/// Cached max_lights from the last successful Initialize call.
	uint32_t _capacity = 0;
	/// Cached VkDeviceAddress of _buffer.
	VkDeviceAddress _bufferAddress = 0;
	/// Count of records written by the most recent UploadFrame call.
	uint32_t _lightCount = 0;
};

} // namespace virasa

#endif // VIRASA_RENDERER_LIGHTING_LIGHTTABLE_H
