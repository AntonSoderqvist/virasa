#ifndef VIRASA_RENDERER_GRAPH_PASS_H
#define VIRASA_RENDERER_GRAPH_PASS_H

#include <functional>
#include <span>
#include <vector>

#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/graph/BufferRegistry.h"
#include "virasa/renderer/graph/ImageRegistry.h"
#include "virasa/renderer/graph/Types.h"
#include "vulkan/vulkan.h"

namespace virasa::renderer::graph
{
/**
 * @brief Describes one color attachment used by a pass.
 */
struct ColorAttachmentInfo
{
	public:
	ImageHandle image;
	LoadOp loadOp = LoadOp::DontCare;
	StoreOp storeOp = StoreOp::Store;
	ClearColor clearColor;
};

/**
 * @brief Describes the optional depth attachment used by a pass.
 */
struct DepthAttachmentInfo
{
	public:
	ImageHandle image;
	LoadOp loadOp = LoadOp::DontCare;
	StoreOp storeOp = StoreOp::DontCare;
	float clearDepth = 1.0f;
	bool present = false;
};

/**
 * @brief Describes one non-attachment image access performed by a pass.
 */
struct ImageAccess
{
	public:
	ImageHandle image;
	ResourceUsage usage = ResourceUsage::Undefined;
};

/**
 * @brief Describes one buffer access performed by a pass.
 */
struct BufferAccess
{
	public:
	BufferHandle buffer;
	ResourceUsage usage = ResourceUsage::Undefined;
};

/**
 * @brief Provides record-time access to graph resources for a pass callback.
 */
class GraphContext final
{
	public:
	GraphContext() = delete;
	GraphContext(const GraphContext&) = delete;
	GraphContext(GraphContext&&) = delete;
	GraphContext& operator=(const GraphContext&) = delete;
	GraphContext& operator=(GraphContext&&) = delete;

	GraphContext(VkCommandBuffer command_buffer, VkExtent2D render_extent,
		const ImageRegistry& image_registry, const BufferRegistry& buffer_registry,
		const Device& device) noexcept;

	/**
	 * @brief Returns the command buffer the pass should record into.
	 * @return The active Vulkan command buffer.
	 */
	[[nodiscard]] VkCommandBuffer GetCommandBuffer() const noexcept;

	/**
	 * @brief Returns the render extent for the current pass.
	 * @return The render-area extent.
	 */
	[[nodiscard]] VkExtent2D GetRenderExtent() const noexcept;

	/**
	 * @brief Resolves an image handle to its image view.
	 * @param handle The image handle to resolve.
	 * @return The Vulkan image view for the handle.
	 */
	[[nodiscard]] VkImageView GetImageView(ImageHandle handle) const noexcept;

	/**
	 * @brief Resolves an image handle to its image handle.
	 * @param handle The image handle to resolve.
	 * @return The Vulkan image handle for the resource.
	 */
	[[nodiscard]] VkImage GetImage(ImageHandle handle) const noexcept;

	/**
	 * @brief Resolves a buffer handle to its buffer handle.
	 * @param handle The buffer handle to resolve.
	 * @return The Vulkan buffer handle for the resource.
	 */
	[[nodiscard]] VkBuffer GetBuffer(BufferHandle handle) const noexcept;

	/**
	 * @brief Resolves a buffer handle to its device address.
	 * @param handle The buffer handle to resolve.
	 * @return The Vulkan device address for the resource.
	 */
	[[nodiscard]] VkDeviceAddress GetBufferAddress(BufferHandle handle) const noexcept;

	private:
	VkCommandBuffer _commandBuffer = VK_NULL_HANDLE;
	VkExtent2D _renderExtent{};
	const ImageRegistry* _imageRegistry = nullptr;
	const BufferRegistry* _bufferRegistry = nullptr;
	const Device* _device = nullptr;
};

/**
 * @brief Holds the declarative state for one render-graph pass.
 */
class Pass final
{
	public:
	Pass() = default;
	Pass(const Pass&) = delete;
	Pass(Pass&& other) noexcept;
	Pass& operator=(const Pass&) = delete;
	Pass& operator=(Pass&& other) noexcept;

	/**
	 * @brief Returns the pass name.
	 * @return Borrowed pass name pointer, or nullptr when unset.
	 */
	[[nodiscard]] const char* GetName() const noexcept;

	/**
	 * @brief Returns the pass color attachments.
	 * @return Span over the stored color attachments.
	 */
	[[nodiscard]] std::span<const ColorAttachmentInfo> GetColorAttachments() const noexcept;

	/**
	 * @brief Reports whether the pass has a depth attachment.
	 * @return True when a depth attachment is present.
	 */
	[[nodiscard]] bool HasDepthAttachment() const noexcept;

	/**
	 * @brief Returns the stored depth attachment description.
	 * @return Reference to the stored depth attachment info.
	 */
	[[nodiscard]] const DepthAttachmentInfo& GetDepthAttachment() const noexcept;

	/**
	 * @brief Returns all image accesses declared by the pass.
	 * @return Span over the concatenated image access list.
	 */
	[[nodiscard]] std::span<const ImageAccess> GetImageAccesses() const noexcept;

	/**
	 * @brief Returns all buffer accesses declared by the pass.
	 * @return Span over the concatenated buffer access list.
	 */
	[[nodiscard]] std::span<const BufferAccess> GetBufferAccesses() const noexcept;

	/**
	 * @brief Reports whether a record callback has been assigned.
	 * @return True when the callback is non-empty.
	 */
	[[nodiscard]] bool HasRecordCallback() const noexcept;

	/**
	 * @brief Invokes the pass record callback.
	 * @param ctx Record-time graph context.
	 */
	void Invoke(const GraphContext& ctx) const;

	private:
	friend class PassBuilder;

	const char* _name = nullptr;
	std::vector<ColorAttachmentInfo> _colorAttachments;
	DepthAttachmentInfo _depthAttachment;
	std::vector<ImageAccess> _imageReads;
	std::vector<ImageAccess> _imageWrites;
	std::vector<ImageAccess> _imageAccesses;
	std::vector<BufferAccess> _bufferReads;
	std::vector<BufferAccess> _bufferWrites;
	std::vector<BufferAccess> _bufferAccesses;
	std::function<void(const GraphContext&)> _recordCallback;

	void RebuildImageAccesses();
	void RebuildBufferAccesses();
	void ResetMovedFrom() noexcept;
};

/**
 * @brief Fluent authoring surface for populating a pass.
 */
class PassBuilder final
{
	public:
	PassBuilder() = delete;
	PassBuilder(const PassBuilder&) = delete;
	PassBuilder(PassBuilder&&) = delete;
	PassBuilder& operator=(const PassBuilder&) = delete;
	PassBuilder& operator=(PassBuilder&&) = delete;

	explicit PassBuilder(Pass& pass, const char* name = nullptr) noexcept;

	/**
	 * @brief Appends a color attachment to the pass.
	 * @param image Image handle used as the color attachment.
	 * @param loadOp Load operation for the attachment.
	 * @param clearColor Clear color used when loadOp is Clear.
	 * @return This builder.
	 */
	PassBuilder& ColorAttachment(ImageHandle image, LoadOp loadOp, ClearColor clearColor);

	/**
	 * @brief Sets the depth attachment for the pass.
	 * @param image Image handle used as the depth attachment.
	 * @param loadOp Load operation for the attachment.
	 * @param clearDepth Clear depth used when loadOp is Clear.
	 * @return This builder.
	 */
	PassBuilder& DepthAttachment(ImageHandle image, LoadOp loadOp, float clearDepth);

	/**
	 * @brief Declares an image read by the pass.
	 * @param image Image handle to read.
	 * @param usage Declared usage for the image.
	 * @return This builder.
	 */
	PassBuilder& Read(ImageHandle image, ResourceUsage usage);

	/**
	 * @brief Declares a buffer read by the pass.
	 * @param buffer Buffer handle to read.
	 * @param usage Declared usage for the buffer.
	 * @return This builder.
	 */
	PassBuilder& Read(BufferHandle buffer, ResourceUsage usage);

	/**
	 * @brief Declares an image write by the pass.
	 * @param image Image handle to write.
	 * @param usage Declared usage for the image.
	 * @return This builder.
	 */
	PassBuilder& Write(ImageHandle image, ResourceUsage usage);

	/**
	 * @brief Declares a buffer write by the pass.
	 * @param buffer Buffer handle to write.
	 * @param usage Declared usage for the buffer.
	 * @return This builder.
	 */
	PassBuilder& Write(BufferHandle buffer, ResourceUsage usage);

	/**
	 * @brief Sets the pass record callback.
	 * @param callback Callback invoked during graph execution.
	 * @return This builder.
	 */
	PassBuilder& Record(std::function<void(const GraphContext&)> callback);

	private:
	Pass* _pass = nullptr;
};
} // namespace virasa::renderer::graph

#endif
