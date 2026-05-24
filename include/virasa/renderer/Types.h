#ifndef VIRASA_RENDERER_TYPES_H
#define VIRASA_RENDERER_TYPES_H

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <span>
#include <vector>

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

namespace virasa
{

/**
 * @brief Error codes for renderer operations.
 *
 * RenderError values represent distinct failure modes for renderer operations.
 * None is the only success value.
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
	/// @brief vkCreateGraphicsPipelines returned a VkResult other than VK_SUCCESS, or required
	/// inputs were missing.
	PipelineCreateFailed,
	/// @brief vkCreateCommandPool or vkAllocateCommandBuffers returned a VkResult other than
	/// VK_SUCCESS.
	CommandPoolCreateFailed,
	/// @brief vkCreateSemaphore or vkCreateFence returned a VkResult other than VK_SUCCESS during
	/// per-frame sync object creation.
	FenceCreateFailed,
	/// @brief vkCreateImage or vkBindImageMemory returned a VkResult other than VK_SUCCESS.
	ImageCreateFailed,
	/// @brief No suitable memory type was found, or vkAllocateMemory returned a VkResult other
	/// than VK_SUCCESS.
	MemoryAllocFailed,
	/// @brief vkCreateBuffer or vkBindBufferMemory returned a VkResult other than VK_SUCCESS.
	BufferCreateFailed,
	/// @brief vkMapMemory against a host-visible allocation returned a VkResult other than
	/// VK_SUCCESS.
	MemoryMapFailed,
	/// @brief vkCreateSampler returned a VkResult other than VK_SUCCESS.
	SamplerCreateFailed,
	/// @brief vkCreateDescriptorPool, vkCreateDescriptorSetLayout, or vkAllocateDescriptorSets
	/// returned a VkResult other than VK_SUCCESS.
	DescriptorPoolCreateFailed
};

/**
 * @brief Status codes for swapchain operations.
 *
 * Success indicates a successful operation. Recreated indicates the swapchain
 * needs to be recreated. Error indicates an unrecoverable failure. NotReady
 * indicates the frame loop should skip rendering this iteration.
 */
enum class SwapchainStatus : uint8_t
{
	Success = 0,
	Recreated,
	Error,
	NotReady
};

/**
 * @brief Renderer-wide configuration shared by all renderer subsystems.
 *
 * Holds settings such as application name, validation, instance extensions,
 * swapchain usage flags, and frame buffering.
 */
struct RendererConfig
{
	/** @brief Application name passed to Vulkan. */
	const char* applicationName = "Virasa";

	/** @brief Application version passed to Vulkan. */
	uint32_t applicationVersion = 0;

	/** @brief Whether to enable validation layers. */
	bool enableValidation = true;

	/** @brief Array of required instance extension names. */
	const char* const* requiredInstanceExtensions = nullptr;

	/** @brief Number of required instance extensions. */
	uint32_t requiredInstanceExtensionCount = 0;

	/** @brief Prefer VK_PRESENT_MODE_MAILBOX_KHR over FIFO. */
	bool preferMailbox = false;

	/** @brief VkImageUsageFlags for the swapchain images. */
	VkImageUsageFlags swapchainImageUsage =
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	/** @brief Number of frames that may be in flight concurrently. */
	uint32_t maxFramesInFlight = 2;

	/** @brief Depth format used for depth attachments. */
	VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
};

/**
 * @brief Holds Vulkan queue family indices and their availability flags.
 *
 * Used to select queue families for graphics, present, and transfer operations.
 */
struct QueueFamilies
{
	/** @brief Graphics queue family index. */
	uint32_t graphicsFamily = 0;

	/** @brief Present queue family index. */
	uint32_t presentFamily = 0;

	/** @brief Transfer queue family index. */
	uint32_t transferFamily = 0;

	/** @brief Whether a graphics queue family was found. */
	bool graphicsFound = false;

	/** @brief Whether a present queue family was found. */
	bool presentFound = false;

	/** @brief Whether a dedicated transfer queue family was found. */
	bool dedicatedTransfer = false;

	/**
	 * @brief Returns true if both graphics and present families are found.
	 */
	[[nodiscard]] bool IsComplete() const noexcept
	{
		return graphicsFound && presentFound;
	}

	/**
	 * @brief Returns true if the graphics and present families are the same.
	 */
	[[nodiscard]] bool IsSameFamily() const noexcept
	{
		return graphicsFamily == presentFamily;
	}
};

/**
 * @brief Describes one vertex attribute for a vertex shader.
 *
 * Specifies the shader location, format, and byte offset of the attribute.
 */
struct VertexAttribute
{
	/** @brief Shader location index. */
	uint32_t location = 0;

	/** @brief Vulkan format of the attribute. */
	VkFormat format = VK_FORMAT_UNDEFINED;

	/** @brief Byte offset from the start of the vertex record. */
	uint32_t offset = 0;
};

/**
 * @brief Describes the vertex input layout for a single binding.
 *
 * Contains the stride, input rate, and a span of attribute descriptors.
 */
struct VertexLayout
{
	/** @brief Size in bytes of one vertex record. */
	uint32_t stride = 0;

	/** @brief Per-vertex or per-instance stepping. */
	VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	/** @brief Non-owning span of vertex attribute descriptors. */
	std::span<const VertexAttribute> attributes;
};

/**
 * @brief A single vertex with position, normal, and texture coordinates.
 *
 * The memory layout is tightly packed: 3 floats for position, 3 for normal,
 * 2 for UV, total 32 bytes with no padding.
 */
struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv;
};

/**
 * @brief CPU-side geometry data with vertices and indices.
 *
 * Owns vectors of Vertex and uint32_t indices.
 */
struct MeshData
{
	/** @brief Vertex array. */
	std::vector<Vertex> vertices;

	/** @brief Index array. */
	std::vector<uint32_t> indices;
};

/**
 * @brief Writes the string representation of a RenderError to an ostream.
 *
 * For known values, writes the identifier (e.g., "None"). For unknown values,
 * writes "RenderError(N)" where N is the decimal value.
 */
inline std::ostream& operator<<(std::ostream& os, RenderError error)
{
	switch (error)
	{
		case RenderError::None:
			return os << "None";
		case RenderError::AlreadyInitialized:
			return os << "AlreadyInitialized";
		case RenderError::NotInitialized:
			return os << "NotInitialized";
		case RenderError::VulkanNotAvailable:
			return os << "VulkanNotAvailable";
		case RenderError::InstanceCreateFailed:
			return os << "InstanceCreateFailed";
		case RenderError::SurfaceCreateFailed:
			return os << "SurfaceCreateFailed";
		case RenderError::NoSuitableDevice:
			return os << "NoSuitableDevice";
		case RenderError::DeviceCreateFailed:
			return os << "DeviceCreateFailed";
		case RenderError::SwapchainCreateFailed:
			return os << "SwapchainCreateFailed";
		case RenderError::ImageViewCreateFailed:
			return os << "ImageViewCreateFailed";
		case RenderError::ShaderLoadFailed:
			return os << "ShaderLoadFailed";
		case RenderError::ShaderCreateFailed:
			return os << "ShaderCreateFailed";
		case RenderError::PipelineLayoutCreateFailed:
			return os << "PipelineLayoutCreateFailed";
		case RenderError::PipelineCreateFailed:
			return os << "PipelineCreateFailed";
		case RenderError::CommandPoolCreateFailed:
			return os << "CommandPoolCreateFailed";
		case RenderError::FenceCreateFailed:
			return os << "FenceCreateFailed";
		case RenderError::ImageCreateFailed:
			return os << "ImageCreateFailed";
		case RenderError::MemoryAllocFailed:
			return os << "MemoryAllocFailed";
		case RenderError::BufferCreateFailed:
			return os << "BufferCreateFailed";
		case RenderError::MemoryMapFailed:
			return os << "MemoryMapFailed";
		case RenderError::SamplerCreateFailed:
			return os << "SamplerCreateFailed";
		case RenderError::DescriptorPoolCreateFailed:
			return os << "DescriptorPoolCreateFailed";
		default:
			return os << "RenderError(" << static_cast<int>(error) << ")";
	}
}

} // namespace virasa

#endif // VIRASA_RENDERER_TYPES_H