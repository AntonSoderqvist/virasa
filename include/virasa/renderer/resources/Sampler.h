#ifndef VIRASA_RENDERER_RESOURCES_SAMPLER_H
#define VIRASA_RENDERER_RESOURCES_SAMPLER_H

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "vulkan/vulkan.h"

namespace virasa
{

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
