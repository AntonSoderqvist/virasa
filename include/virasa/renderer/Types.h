#ifndef VIRASA_RENDERER_TYPES_H
#define VIRASA_RENDERER_TYPES_H

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <span>
#include <vector>
#include <type_traits>

#include "virasa/math/Types.h"

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
 * @brief Identifies which camera domain a camera belongs to.
 *
 * Main identifies the primary runtime camera. Editor identifies a camera
 * owned by an editor or authoring tool.
 */
enum class CameraDomain : uint8_t
{
	Main = 0,
	Editor
};

/**
 * @brief Error codes for renderer resource registration operations.
 *
 * RegisterError values represent coarse-grained failure modes reported by
 * authoring-facing registration APIs. None is the only success value.
 */
enum class RegisterError : uint8_t
{
	None = 0,
	OutOfSlots,
	InvalidInput,
	UploadFailed,
	SamplerCreateFailed
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
 * @brief A single vertex in the renderer's fixed geometry format.
 *
 * The memory layout is tightly packed: 3 floats for position, 3 for normal,
 * 4 for tangent, and 2 for UV, total 48 bytes with no padding.
 */
struct Vertex
{
	/** @brief Vertex position. */
	virasa::math::Vec3 position;

	/** @brief Vertex normal. */
	virasa::math::Vec3 normal;

	/** @brief Vertex tangent and handedness sign. */
	virasa::math::Vec4 tangent;

	/** @brief Vertex texture coordinates. */
	virasa::math::Vec2 uv;
};

static_assert(offsetof(Vertex, position) == 0, "Vertex::position must be at offset 0.");
static_assert(offsetof(Vertex, normal) == 12, "Vertex::normal must be at offset 12.");
static_assert(offsetof(Vertex, tangent) == 24, "Vertex::tangent must be at offset 24.");
static_assert(offsetof(Vertex, uv) == 40, "Vertex::uv must be at offset 40.");
static_assert(sizeof(Vertex) == 48, "Vertex must remain tightly packed at 48 bytes.");
static_assert(std::is_standard_layout_v<Vertex>, "Vertex must be standard layout.");

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
 * @brief Describes Vulkan sampler parameters used by renderer texture registration.
 *
 * SamplerConfig is a plain data key type used for sampler caching.
 */
struct SamplerConfig
{
	/** @brief Magnification filter. */
	VkFilter magFilter = VK_FILTER_LINEAR;

	/** @brief Minification filter. */
	VkFilter minFilter = VK_FILTER_LINEAR;

	/** @brief Mipmap sampling mode. */
	VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	/** @brief Address mode for the U axis. */
	VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	/** @brief Address mode for the V axis. */
	VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	/** @brief Address mode for the W axis. */
	VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	/** @brief Whether anisotropic filtering is enabled. */
	bool anisotropyEnable = false;

	/** @brief Maximum anisotropy value. */
	float maxAnisotropy = 1.0f;

	/** @brief Minimum LOD clamp. */
	float minLod = 0.0f;

	/** @brief Maximum LOD clamp. */
	float maxLod = VK_LOD_CLAMP_NONE;

	/** @brief LOD bias. */
	float mipLodBias = 0.0f;

	/** @brief Border color for CLAMP_TO_BORDER address modes. */
	VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

	/**
	 * @brief Compares two sampler configurations for equality.
	 * @param other The other sampler configuration.
	 * @return True if all members compare equal; otherwise false.
	 */
	[[nodiscard]] bool operator==(const SamplerConfig& other) const noexcept = default;
};

/**
 * @brief Describes one sampled texture upload and registration request.
 *
 * TextureUpload is a non-owning view of source pixel bytes and associated
 * metadata needed to create a sampled 2D texture.
 */
struct TextureUpload
{
	/** @brief Tightly packed source pixel bytes in scanline order. */
	std::span<const std::byte> pixels;

	/** @brief Texture width in texels. */
	uint32_t width = 0u;

	/** @brief Texture height in texels. */
	uint32_t height = 0u;

	/** @brief Vulkan format of the source texels and destination image. */
	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

	/** @brief Sampler configuration for the registered texture. */
	SamplerConfig sampler;
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