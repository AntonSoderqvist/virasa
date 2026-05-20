#ifndef VIRASA_RENDERER_CORE_SWAPCHAIN_H
#define VIRASA_RENDERER_CORE_SWAPCHAIN_H

#include <cstdint>
#include <span>
#include <vector>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Surface.h"
#include "virasa/window/Types.h"
#include "vulkan/vulkan.h"

namespace virasa
{

/**
 * @brief Manages a VkSwapchainKHR, its images, and image views.
 *
 * Swapchain owns at most one VkSwapchainKHR handle and one VkImageView per
 * swapchain image. It is default-constructible, movable, and non-copyable.
 * A default-constructed Swapchain owns no Vulkan handles and is not initialized.
 */
class Swapchain final
{
	public:
	/// @brief Default-constructs a Swapchain that owns no Vulkan handles.
	Swapchain() noexcept = default;

	/// @brief Destroys all owned image views and the owned VkSwapchainKHR.
	~Swapchain();

	Swapchain(const Swapchain&) = delete;
	Swapchain& operator=(const Swapchain&) = delete;

	/// @brief Move-constructs, transferring ownership of all Vulkan handles.
	Swapchain(Swapchain&& other) noexcept;

	/// @brief Move-assigns, destroying existing handles then taking ownership.
	Swapchain& operator=(Swapchain&& other) noexcept;

	/**
	 * @brief Creates the VkSwapchainKHR, retrieves images, and creates image views.
	 * @param device An initialized Device whose VkDevice handle is borrowed.
	 * @param surface A Surface for which AreFormatsQueried returns true.
	 * @param window_size The current window pixel dimensions.
	 * @param config Renderer-wide configuration (image usage, etc.).
	 * @return RenderError::None on success, or a specific error code on failure.
	 */
	[[nodiscard]] RenderError Initialize(const Device& device, const Surface& surface,
		virasa::Size2D window_size, const RendererConfig& config);

	/**
	 * @brief Rebuilds the swapchain after a resize or suboptimal condition.
	 * @param device An initialized Device.
	 * @param surface A Surface (taken by non-const ref to allow RefreshCapabilities).
	 * @param window_size The new window pixel dimensions.
	 * @param config Renderer-wide configuration.
	 * @return RenderError::None on success, or a specific error code on failure.
	 */
	[[nodiscard]] RenderError Recreate(const Device& device, Surface& surface,
		virasa::Size2D window_size, const RendererConfig& config);

	/**
	 * @brief Acquires the next presentable image from the swapchain.
	 * @param device The Device used to call vkAcquireNextImageKHR.
	 * @param signal_semaphore Semaphore to signal when the image is available.
	 * @param out_image_index Non-null pointer that receives the image index.
	 * @return SwapchainStatus::Success, Recreated, or Error.
	 */
	[[nodiscard]] SwapchainStatus AcquireNextImage(
		const Device& device, VkSemaphore signal_semaphore, uint32_t* out_image_index);

	/**
	 * @brief Presents a rendered image to the surface.
	 * @param present_queue The presentation-capable VkQueue.
	 * @param wait_semaphore Semaphore to wait on before presenting.
	 * @param image_index Index of the image to present.
	 * @return SwapchainStatus::Success, Recreated, or Error.
	 */
	[[nodiscard]] SwapchainStatus Present(
		VkQueue present_queue, VkSemaphore wait_semaphore, uint32_t image_index);

	/**
	 * @brief Returns the VkFormat of the swapchain images.
	 * @return The cached VkFormat, or VK_FORMAT_UNDEFINED if not initialized.
	 */
	[[nodiscard]] VkFormat GetFormat() const noexcept;

	/**
	 * @brief Returns the VkExtent2D of the swapchain.
	 * @return The cached extent, or {0, 0} if not initialized.
	 */
	[[nodiscard]] VkExtent2D GetExtent() const noexcept;

	/**
	 * @brief Returns a span over the created VkImageView handles.
	 * @return Span of image views; empty if not initialized.
	 */
	[[nodiscard]] std::span<const VkImageView> GetImageViews() const noexcept;

	/**
	 * @brief Returns a span over the retrieved VkImage handles.
	 * @return Span of images; empty if not initialized.
	 */
	[[nodiscard]] std::span<const VkImage> GetImages() const noexcept;

	/**
	 * @brief Returns the number of swapchain images.
	 * @return Image count, or 0 if not initialized.
	 */
	[[nodiscard]] uint32_t GetImageCount() const noexcept;

	/**
	 * @brief Returns true if this Swapchain currently owns a VkSwapchainKHR.
	 * @return true when initialized, false otherwise.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

	private:
	/// Borrowed VkDevice handle (not owned).
	VkDevice _device = VK_NULL_HANDLE;

	/// Owned VkSwapchainKHR handle.
	VkSwapchainKHR _swapchain = VK_NULL_HANDLE;

	/// Retrieved VkImage handles (owned by the VkSwapchainKHR, not destroyed separately).
	std::vector<VkImage> _images;

	/// Created VkImageView handles (owned by this Swapchain).
	std::vector<VkImageView> _imageViews;

	/// Cached format from the last successful Initialize or Recreate.
	VkFormat _format = VK_FORMAT_UNDEFINED;

	/// Cached extent from the last successful Initialize or Recreate.
	VkExtent2D _extent = {0, 0};

	/// Destroys all owned image views and the swapchain handle, then resets to default state.
	void Destroy();

	/// Creates the swapchain and image views; used by both Initialize and Recreate.
	RenderError CreateSwapchainAndViews(const Surface& surface, virasa::Size2D window_size,
		const RendererConfig& config, VkSwapchainKHR old_swapchain);
};

} // namespace virasa

#endif // VIRASA_RENDERER_CORE_SWAPCHAIN_H
