#ifndef VIRASA_RENDERER_RESOURCES_IMAGE_H
#define VIRASA_RENDERER_RESOURCES_IMAGE_H

#include <cstdint>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "vulkan/vulkan.h"

namespace virasa
{

/**
 * @brief Configuration parameters for creating a VkImage, its backing memory, and its VkImageView.
 */
struct ImageConfig
{
	public:
	/// Width of the image in pixels.
	uint32_t width = 0;

	/// Height of the image in pixels.
	uint32_t height = 0;

	/// Vulkan format for both the VkImage and its VkImageView.
	VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;

	/// Usage flags for the VkImageCreateInfo.
	VkImageUsageFlags usage = 0;

	/// Aspect mask for the VkImageView subresource range.
	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

	/// Memory property flags the backing allocation must satisfy.
	VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	/// Flags for the VkImageCreateInfo.
	VkImageCreateFlags flags = 0;
};

/**
 * @brief Owns a VkImage, its backing VkDeviceMemory, and a VkImageView over it.
 *
 * Image is RAII: it creates and destroys Vulkan resources automatically.
 * It is not copyable but is movable. A default-constructed Image owns no
 * Vulkan resources and IsInitialized() returns false.
 */
class Image final
{
	public:
	/// Default constructor. Owns no resources; IsInitialized() returns false.
	Image() = default;

	/// Destructor. Destroys owned VkImageView, VkImage, and VkDeviceMemory.
	~Image();

	Image(const Image&) = delete;
	Image& operator=(const Image&) = delete;

	/// Move constructor. Transfers ownership from source; source is left in default-constructed
	/// state.
	Image(Image&& other) noexcept;

	/// Move assignment. Destroys current resources, then takes ownership from source.
	Image& operator=(Image&& other) noexcept;

	/**
	 * @brief Creates the VkImage, allocates and binds backing VkDeviceMemory, and creates the
	 * VkImageView.
	 * @param device A fully-initialized Device to borrow the VkDevice from.
	 * @param config Parameters describing the image to create.
	 * @return RenderError::None on success, or a specific error code on failure.
	 */
	[[nodiscard]] RenderError Initialize(const Device& device, const ImageConfig& config);

	/**
	 * @brief Recreates the image at a new resolution, preserving all other config fields.
	 * @param new_width New width in pixels.
	 * @param new_height New height in pixels.
	 * @return RenderError::None on success, or a specific error code on failure.
	 */
	[[nodiscard]] RenderError Resize(uint32_t new_width, uint32_t new_height);

	/**
	 * @brief Returns the owned VkImage handle, or VK_NULL_HANDLE if not initialized.
	 * @return The VkImage handle.
	 */
	[[nodiscard]] VkImage GetHandle() const noexcept;

	/**
	 * @brief Returns the owned VkImageView handle, or VK_NULL_HANDLE if not initialized.
	 * @return The VkImageView handle.
	 */
	[[nodiscard]] VkImageView GetView() const noexcept;

	/**
	 * @brief Returns the cached image format.
	 * @return The VkFormat from the last successful Initialize.
	 */
	[[nodiscard]] VkFormat GetFormat() const noexcept;

	/**
	 * @brief Returns the cached image width in pixels.
	 * @return The width.
	 */
	[[nodiscard]] uint32_t GetWidth() const noexcept;

	/**
	 * @brief Returns the cached image height in pixels.
	 * @return The height.
	 */
	[[nodiscard]] uint32_t GetHeight() const noexcept;

	/**
	 * @brief Returns the cached image extent as a VkExtent2D.
	 * @return VkExtent2D with width and height.
	 */
	[[nodiscard]] VkExtent2D GetExtent() const noexcept;

	/**
	 * @brief Returns true if and only if this Image currently owns a valid VkImage.
	 * @return True if initialized, false otherwise.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

	private:
	/// Destroys owned resources if any; safe to call on a default-constructed or moved-from
	/// Image.
	void Teardown();

	/// Selects a memory type index satisfying typeFilter and required property flags.
	/// Returns UINT32_MAX if no suitable type is found.
	uint32_t SelectMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags required) const;

	/// Runs creation steps 1-4 using the current cached config and memory properties.
	RenderError CreateResources();

	VkDevice _device = VK_NULL_HANDLE;
	VkImage _image = VK_NULL_HANDLE;
	VkDeviceMemory _memory = VK_NULL_HANDLE;
	VkImageView _view = VK_NULL_HANDLE;
	ImageConfig _config = {};
	VkPhysicalDeviceMemoryProperties _memProps = {};
};

} // namespace virasa

#endif // VIRASA_RENDERER_RESOURCES_IMAGE_H
