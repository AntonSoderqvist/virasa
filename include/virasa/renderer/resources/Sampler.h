#ifndef VIRASA_RENDERER_RESOURCES_SAMPLER_H
#define VIRASA_RENDERER_RESOURCES_SAMPLER_H

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "vulkan/vulkan.h"

namespace virasa
{

/**
 * @brief Configuration parameters for creating a VkSampler.
 *
 * All fields have sensible defaults and are passed verbatim into
 * VkSamplerCreateInfo during Sampler::Initialize.
 */
struct SamplerConfig
{
	public:
	/// Magnification filter. Default: VK_FILTER_LINEAR.
	VkFilter magFilter = VK_FILTER_LINEAR;

	/// Minification filter. Default: VK_FILTER_LINEAR.
	VkFilter minFilter = VK_FILTER_LINEAR;

	/// Mipmap filter mode. Default: VK_SAMPLER_MIPMAP_MODE_LINEAR.
	VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	/// Address mode for U axis. Default: VK_SAMPLER_ADDRESS_MODE_REPEAT.
	VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	/// Address mode for V axis. Default: VK_SAMPLER_ADDRESS_MODE_REPEAT.
	VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	/// Address mode for W axis. Default: VK_SAMPLER_ADDRESS_MODE_REPEAT.
	VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	/// Enable anisotropic filtering. Default: false.
	bool anisotropyEnable = false;

	/// Maximum anisotropy level (used when anisotropyEnable is true). Default: 1.0f.
	float maxAnisotropy = 1.0f;

	/// Minimum LOD clamp. Default: 0.0f.
	float minLod = 0.0f;

	/// Maximum LOD clamp. Default: VK_LOD_CLAMP_NONE.
	float maxLod = VK_LOD_CLAMP_NONE;

	/// LOD bias. Default: 0.0f.
	float mipLodBias = 0.0f;

	/// Border color for CLAMP_TO_BORDER address modes. Default: VK_BORDER_COLOR_INT_OPAQUE_BLACK.
	VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
};

/**
 * @brief Owns and manages the lifetime of a single VkSampler handle.
 *
 * Sampler is a RAII wrapper around VkSampler. It is default-constructible
 * (producing an uninitialized, no-op object), movable, and not copyable.
 * Call Initialize to create the underlying Vulkan sampler object.
 * The borrowed VkDevice must outlive any initialized Sampler.
 */
class Sampler final
{
	public:
	/**
	 * @brief Default-constructs a Sampler that owns no Vulkan handle.
	 *
	 * IsInitialized() returns false and GetHandle() returns VK_NULL_HANDLE
	 * until Initialize is called successfully.
	 */
	Sampler() noexcept = default;

	/// Destroys the owned VkSampler, if any.
	~Sampler() noexcept;

	// Not copyable.
	Sampler(const Sampler&) = delete;
	Sampler& operator=(const Sampler&) = delete;

	/**
	 * @brief Move-constructs a Sampler, transferring ownership from @p other.
	 *
	 * After the move, @p other is left in the same observable state as a
	 * default-constructed Sampler.
	 */
	Sampler(Sampler&& other) noexcept;

	/**
	 * @brief Move-assigns a Sampler, transferring ownership from @p other.
	 *
	 * If this Sampler already owns a handle it is destroyed first.
	 * After the move, @p other is left in the same observable state as a
	 * default-constructed Sampler.
	 * @return Reference to this.
	 */
	Sampler& operator=(Sampler&& other) noexcept;

	/**
	 * @brief Creates the VkSampler from the supplied configuration.
	 *
	 * If this Sampler already owns a handle it is destroyed before the new
	 * one is created. The Device reference is borrowed (not owned) and must
	 * outlive this Sampler.
	 *
	 * @param device An initialized Device whose VkDevice is used.
	 * @param config Parameters for VkSamplerCreateInfo.
	 * @return RenderError::None on success; RenderError::SamplerCreateFailed
	 *         if vkCreateSampler fails.
	 */
	[[nodiscard]] RenderError Initialize(const Device& device, const SamplerConfig& config);

	/**
	 * @brief Returns the owned VkSampler handle, or VK_NULL_HANDLE.
	 * @return The VkSampler handle, or VK_NULL_HANDLE if not initialized.
	 */
	[[nodiscard]] VkSampler GetHandle() const noexcept;

	/**
	 * @brief Returns true if this Sampler currently owns a VkSampler.
	 * @return true if initialized, false otherwise.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

	private:
	/// Destroys the owned VkSampler if present; resets all handles to null.
	void Teardown() noexcept;

	VkSampler _sampler = VK_NULL_HANDLE;
	VkDevice _device = VK_NULL_HANDLE;
};

} // namespace virasa

#endif // VIRASA_RENDERER_RESOURCES_SAMPLER_H
