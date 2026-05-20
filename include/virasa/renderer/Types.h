#ifndef VIRASA_RENDERER_TYPES_H
#define VIRASA_RENDERER_TYPES_H

#include <cstdint>
#include <span>
#include "vulkan/vulkan.h"

namespace virasa
{

/**
 * @brief Error codes returned by renderer operations.
 *
 * RenderError::None is the unique success value. All other values represent
 * distinct failure modes. The underlying type is uint8_t.
 */
enum class RenderError : uint8_t
{
	None = 0,
	AlreadyInitialized,
	NotInitialized,
	VulkanNotAvailable,
	InstanceCreateFailed,
	SurfaceCreateFailed,
	/// @brief No VkPhysicalDevice met the caller's hard requirements for selection.
	NoSuitableDevice,
	/// @brief vkCreateDevice returned a VkResult other than VK_SUCCESS.
	DeviceCreateFailed,
	/// @brief vkCreateSwapchainKHR returned a VkResult other than VK_SUCCESS.
	SwapchainCreateFailed,
	/// @brief vkCreateImageView returned a VkResult other than VK_SUCCESS.
	ImageViewCreateFailed,
	/// @brief A shader source file could not be loaded from disk.
	ShaderLoadFailed,
	/// @brief vkCreateShaderModule returned a VkResult other than VK_SUCCESS.
	ShaderCreateFailed,
	/// @brief vkCreatePipelineLayout returned a VkResult other than VK_SUCCESS.
	PipelineLayoutCreateFailed,
	/// @brief vkCreateGraphicsPipelines returned a VkResult other than VK_SUCCESS, or required inputs were missing.
	PipelineCreateFailed
};

/**
 * @brief Status of a swapchain acquire or present operation.
 *
 * SwapchainStatus is an enum class with underlying type uint8_t.
 * Success is the unique "no action needed" value. Recreated indicates the
 * swapchain is out-of-date and should be recreated before the next frame.
 * Error indicates an unrecoverable failure.
 */
enum class SwapchainStatus : uint8_t
{
	Success = 0,
	Recreated,
	Error
};

/**
 * @brief Renderer-wide configuration shared by every contract in the virasa.renderer subtree.
 *
 * RendererConfig is a plain aggregate struct. All pointer members are non-owning;
 * the caller must keep the referenced data alive for the duration of any call
 * that receives this struct.
 *
 * Consumers that create a VkInstance must set VkApplicationInfo::apiVersion to
 * VK_API_VERSION_1_3.
 */
struct RendererConfig
{
	/// @brief Null-terminated application name passed to Vulkan via VkApplicationInfo.
	const char* applicationName = "Virasa";

	/// @brief Application version passed to Vulkan via VkApplicationInfo.
	uint32_t applicationVersion = 0;

	/// @brief When true, the Khronos validation layer and VK_EXT_debug_utils are requested.
	bool enableValidation = true;

	/**
	 * @brief Pointer to a contiguous array of null-terminated Vulkan instance extension names
	 *        required by the caller. May be nullptr when requiredInstanceExtensionCount is 0.
	 */
	const char* const* requiredInstanceExtensions = nullptr;

	/// @brief Number of strings in the requiredInstanceExtensions array.
	uint32_t requiredInstanceExtensionCount = 0;

	/**
	 * @brief When true, prefer VK_PRESENT_MODE_MAILBOX_KHR over VK_PRESENT_MODE_FIFO_KHR
	 *        if both are supported. Advisory only — falls back to FIFO when mailbox is unavailable.
	 */
	bool preferMailbox = false;

	/**
	 * @brief VkImageUsageFlags passed to VkSwapchainCreateInfoKHR::imageUsage.
	 *
	 * Must include VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT. Callers may add additional
	 * flags (e.g. VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_USAGE_STORAGE_BIT).
	 */
	VkImageUsageFlags swapchainImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
};

/**
 * @brief Holds the Vulkan queue family indices selected for a logical device.
 *
 * QueueFamilies stores the resolved queue family indices together with flags
 * indicating which indices have been found. It owns no resources.
 */
struct QueueFamilies
{
public:
	/**
	 * @brief Returns true if and only if both graphicsFound and presentFound are true.
	 * @return True when both a graphics and a present queue family have been resolved.
	 */
	[[nodiscard]] bool IsComplete() const noexcept
	{
		return graphicsFound && presentFound;
	}

	/**
	 * @brief Returns true if and only if graphicsFamily equals presentFamily.
	 * @return True when the graphics and present queue families share the same index.
	 */
	[[nodiscard]] bool IsSameFamily() const noexcept
	{
		return graphicsFamily == presentFamily;
	}

	/// @brief Queue family index selected for graphics submission.
	uint32_t graphicsFamily = 0;

	/// @brief Queue family index selected for presentation against a VkSurfaceKHR.
	uint32_t presentFamily = 0;

	/// @brief Queue family index selected for transfer-only submission.
	uint32_t transferFamily = 0;

	/// @brief True if a graphics-capable queue family has been resolved.
	bool graphicsFound = false;

	/// @brief True if a present-capable queue family has been resolved.
	bool presentFound = false;

	/// @brief True if a dedicated transfer queue family (no graphics) was found.
	bool dedicatedTransfer = false;
};

/**
 * @brief Describes one vertex attribute consumed by a vertex shader.
 *
 * VertexAttribute is a trivially destructible value type intended to be
 * constructed in arrays describing the layout of a vertex buffer.
 */
struct VertexAttribute
{
public:
	/// @brief Shader location index this attribute binds to (layout(location = N)).
	uint32_t location = 0;

	/// @brief Data format of the attribute (e.g. VK_FORMAT_R32G32B32_SFLOAT).
	VkFormat format = VK_FORMAT_UNDEFINED;

	/// @brief Byte offset of this attribute from the start of a vertex record.
	uint32_t offset = 0;
};

/**
 * @brief Describes the vertex input layout for a single binding consumed by a pipeline.
 *
 * VertexLayout is a non-owning descriptor; the caller must keep the storage
 * referenced by the attributes span alive for the duration of any call that
 * receives the VertexLayout.
 */
struct VertexLayout
{
public:
	/// @brief Size in bytes of one vertex record in the bound buffer.
	uint32_t stride = 0;

	/// @brief Selects per-vertex versus per-instance stepping.
	VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	/// @brief Non-owning span of VertexAttribute records describing individual attributes.
	std::span<const VertexAttribute> attributes;
};

} // namespace virasa

#endif // VIRASA_RENDERER_TYPES_H
