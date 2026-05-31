#ifndef VIRASA_RENDERER_RESOURCES_BINDLESS_TEXTURE_ARRAY_H
#define VIRASA_RENDERER_RESOURCES_BINDLESS_TEXTURE_ARRAY_H

#include <cstdint>
#include <vector>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "vulkan/vulkan.h"

namespace virasa
{

/**
 * @brief Manages a bindless descriptor array of combined image samplers.
 *
 * BindlessTextureArray owns a VkDescriptorPool, VkDescriptorSetLayout, and
 * VkDescriptorSet that back a bindless texture registry (binding 0) and a
 * bindless shadow-map registry (binding 1). Textures and shadow maps are
 * registered and unregistered by independent slot indices. The single
 * VkDescriptorSet is intended to be bound once per frame at set index 0 and
 * remain bound for all draw calls in that frame.
 *
 * BindlessTextureArray borrows (does not own) the VkDevice it is initialized
 * against; the Device must outlive any initialized BindlessTextureArray
 * created against it.
 *
 * This class is not thread-safe per instance.
 */
class BindlessTextureArray final
{
	public:
	/// @brief Default-constructs a BindlessTextureArray that owns no Vulkan handles.
	BindlessTextureArray() noexcept = default;

	/// @brief Destroys the BindlessTextureArray, releasing owned Vulkan handles.
	~BindlessTextureArray();

	BindlessTextureArray(const BindlessTextureArray&) = delete;
	BindlessTextureArray& operator=(const BindlessTextureArray&) = delete;

	/// @brief Move-constructs a BindlessTextureArray, transferring ownership from source.
	BindlessTextureArray(BindlessTextureArray&& other) noexcept;

	/// @brief Move-assigns a BindlessTextureArray, tearing down prior handles then taking
	/// ownership.
	BindlessTextureArray& operator=(BindlessTextureArray&& other) noexcept;

	/**
	 * @brief Creates the descriptor pool, layout, and set backing the bindless texture registry.
	 * @param device An initialized Device with descriptor indexing enabled.
	 * @param max_textures Maximum number of texture slots; must be greater than 0.
	 * @return RenderError::None on success, or RenderError::DescriptorPoolCreateFailed on
	 * failure.
	 */
	[[nodiscard]] RenderError Initialize(const Device& device, uint32_t max_textures);

	/**
	 * @brief Claims a free slot and writes the image_view/sampler into the descriptor set.
	 * @param image_view The VkImageView to register.
	 * @param sampler The VkSampler to register.
	 * @return The claimed slot index, or UINT32_MAX if no slots are available.
	 */
	[[nodiscard]] uint32_t RegisterTexture(VkImageView image_view, VkSampler sampler);

	/**
	 * @brief Returns a previously registered texture slot to the free-slot list.
	 * @param slot A slot index previously returned by RegisterTexture.
	 */
	void UnregisterTexture(uint32_t slot);

	/**
	 * @brief Claims a free shadow-map slot and writes the image_view/sampler into binding 1.
	 * @param image_view The depth VkImageView of the shadow map.
	 * @param sampler A depth-comparison VkSampler.
	 * @return The claimed shadow-map slot index, or UINT32_MAX if no slots are available.
	 */
	[[nodiscard]] uint32_t RegisterShadowMap(VkImageView image_view, VkSampler sampler);

	/**
	 * @brief Returns a previously registered shadow-map slot to the free-slot list.
	 * @param slot A slot index previously returned by RegisterShadowMap.
	 */
	void UnregisterShadowMap(uint32_t slot);

	/**
	 * @brief Returns the owned VkDescriptorSet, or VK_NULL_HANDLE if not initialized.
	 * @return The VkDescriptorSet.
	 */
	[[nodiscard]] VkDescriptorSet GetDescriptorSet() const noexcept;

	/**
	 * @brief Returns the owned VkDescriptorSetLayout (containing both binding 0 and binding 1),
	 *        or VK_NULL_HANDLE if not initialized.
	 * @return The VkDescriptorSetLayout.
	 */
	[[nodiscard]] VkDescriptorSetLayout GetLayout() const noexcept;

	/**
	 * @brief Returns the max_textures value supplied to Initialize, or 0 if not initialized.
	 * @return The maximum number of texture slots.
	 */
	[[nodiscard]] uint32_t GetMaxTextures() const noexcept;

	/**
	 * @brief Returns kMaxShadowMaps when initialized, or 0 if not initialized.
	 * @return The maximum number of shadow-map slots.
	 */
	[[nodiscard]] uint32_t GetMaxShadowMaps() const noexcept;

	/**
	 * @brief Returns true if and only if this BindlessTextureArray owns a VkDescriptorSet.
	 * @return True if initialized.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

	private:
	void Teardown();

	VkDevice _device = VK_NULL_HANDLE;
	VkDescriptorPool _pool = VK_NULL_HANDLE;
	VkDescriptorSetLayout _layout = VK_NULL_HANDLE;
	VkDescriptorSet _descriptorSet = VK_NULL_HANDLE;
	uint32_t _maxTextures = 0;
	std::vector<uint32_t> _freeSlots;
	std::vector<uint32_t> _shadowFreeSlots;
};

/// @brief Fixed compile-time capacity of the shadow-map slot space (binding 1).
inline constexpr uint32_t kMaxShadowMaps = 32;

} // namespace virasa

#endif // VIRASA_RENDERER_RESOURCES_BINDLESS_TEXTURE_ARRAY_H
