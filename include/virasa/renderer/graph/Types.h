#ifndef VIRASA_RENDERER_GRAPH_TYPES_H
#define VIRASA_RENDERER_GRAPH_TYPES_H

#include <cstdint>

#include "vulkan/vulkan.h"

namespace virasa::renderer::graph
{
/**
 * @brief Declares how a graph resource is used by a pass.
 *
 * Each value corresponds to a fixed Vulkan synchronization and image layout state.
 */
enum class ResourceUsage : uint8_t
{
	Undefined = 0,
	ColorAttachment,
	DepthAttachment,
	DepthReadOnly,
	SampledFragment,
	SampledCompute,
	StorageReadCompute,
	StorageWriteCompute,
	TransferSrc,
	TransferDst,
	Present
};

/**
 * @brief Declares how an attachment is treated at the start of a pass.
 */
enum class LoadOp : uint8_t
{
	Load = 0,
	Clear,
	DontCare
};

/**
 * @brief Declares how an attachment is treated at the end of a pass.
 */
enum class StoreOp : uint8_t
{
	Store = 0,
	DontCare
};

/**
 * @brief Typed handle identifying an image resource in the render graph.
 */
struct ImageHandle
{
	public:
	/**
	 * @brief Check whether this handle refers to a valid image slot.
	 * @return True if the handle is valid; otherwise false.
	 */
	[[nodiscard]] bool IsValid() const noexcept
	{
		return id != 0xFFFFFFFFu;
	}

	public:
	uint32_t id = 0xFFFFFFFFu;
};

/**
 * @brief Typed handle identifying a buffer resource in the render graph.
 */
struct BufferHandle
{
	public:
	/**
	 * @brief Check whether this handle refers to a valid buffer slot.
	 * @return True if the handle is valid; otherwise false.
	 */
	[[nodiscard]] bool IsValid() const noexcept
	{
		return id != 0xFFFFFFFFu;
	}

	public:
	uint32_t id = 0xFFFFFFFFu;
};

/**
 * @brief RGBA clear value for color attachments.
 */
struct ClearColor
{
	public:
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	float a = 1.0f;
};

/**
 * @brief Describes a transient image allocation requested by the render graph.
 */
struct GraphImageDesc
{
	public:
	uint32_t width = 0;
	uint32_t height = 0;
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkImageUsageFlags usage = 0;
	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
};

/**
 * @brief Describes a transient buffer allocation requested by the render graph.
 */
struct GraphBufferDesc
{
	public:
	VkDeviceSize size = 0;
	VkBufferUsageFlags usage = 0;
	bool hostVisible = false;
};
} // namespace virasa::renderer::graph

#endif