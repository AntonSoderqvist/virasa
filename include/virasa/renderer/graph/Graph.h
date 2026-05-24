#ifndef VIRASA_RENDERER_GRAPH_GRAPH_H
#define VIRASA_RENDERER_GRAPH_GRAPH_H

#include <cstdint>
#include <vector>

#include "virasa/renderer/graph/BufferRegistry.h"
#include "virasa/renderer/graph/ImageRegistry.h"
#include "virasa/renderer/graph/Pass.h"
#include "virasa/renderer/graph/Types.h"
#include "vulkan/vulkan.h"

namespace virasa
{
class Device;
enum class RenderError : uint8_t;
} // namespace virasa

namespace virasa::renderer::graph
{

/**
 * @brief Per-frame render graph managing pass declarations, resource barriers, and execution.
 *
 * Graph owns a per-frame collection of Pass objects, an import table for externally-owned
 * images, a list of registry-declared handles (released via End), and a bump arena backing
 * PassBuilder objects. Graph borrows (does not own) the Device, ImageRegistry, and
 * BufferRegistry it was initialized against; all three must outlive any initialized Graph.
 *
 * Expected per-frame lifecycle:
 *   Initialize → (Begin → ImportImage/DeclareImage/DeclareBuffer/AddPass* → Compile → Execute →
 * End)*
 */
class Graph final
{
	public:
	/**
	 * @brief Default constructor. Produces an uninitialized Graph with no per-frame state.
	 */
	Graph() = default;

	/// Not copyable.
	Graph(const Graph&) = delete;
	Graph& operator=(const Graph&) = delete;

	/// Movable.
	Graph(Graph&& other) noexcept;
	Graph& operator=(Graph&& other) noexcept;

	~Graph();

	/**
	 * @brief Initialize the Graph against the given device and registries.
	 * @param device       An initialized Device the Graph borrows.
	 * @param image_registry  An initialized ImageRegistry the Graph borrows.
	 * @param buffer_registry An initialized BufferRegistry the Graph borrows.
	 * @return RenderError::None on success.
	 */
	[[nodiscard]] RenderError Initialize(
		const Device& device, ImageRegistry& image_registry, BufferRegistry& buffer_registry);

	/**
	 * @brief Begin a new frame, clearing all per-frame state.
	 */
	void Begin();

	/**
	 * @brief Import an externally-owned image into the current frame.
	 * @param image         The VkImage handle (not owned by the Graph).
	 * @param view          The VkImageView handle (not owned by the Graph).
	 * @param format        The image format.
	 * @param extent        The image extent.
	 * @param current_usage The ResourceUsage the image is currently in.
	 * @return An ImageHandle with the high bit set, valid for this frame only.
	 */
	[[nodiscard]] ImageHandle ImportImage(VkImage image, VkImageView view, VkFormat format,
		VkExtent2D extent, ResourceUsage current_usage);

	/**
	 * @brief Declare a transient image allocated through the ImageRegistry.
	 * @param desc Descriptor for the image to allocate.
	 * @return An ImageHandle naming the allocated slot, or the invalid sentinel on failure.
	 */
	[[nodiscard]] ImageHandle DeclareImage(const GraphImageDesc& desc);

	/**
	 * @brief Declare a transient buffer allocated through the BufferRegistry.
	 * @param desc Descriptor for the buffer to allocate.
	 * @return A BufferHandle naming the allocated slot, or the invalid sentinel on failure.
	 */
	[[nodiscard]] BufferHandle DeclareBuffer(const GraphBufferDesc& desc);

	/**
	 * @brief Append a new pass to the current frame and return a PassBuilder for it.
	 * @param name Non-owning pointer to the pass name; must remain live for the frame duration.
	 * @return Reference to a PassBuilder valid only for the duration of the call chain.
	 */
	PassBuilder& AddPass(const char* name);

	/**
	 * @brief Validate the per-frame pass list and prepare for Execute.
	 * @return RenderError::None on success, RenderError::PipelineCreateFailed on validation
	 * error.
	 */
	[[nodiscard]] RenderError Compile();

	/**
	 * @brief Execute all passes in declaration order, emitting barriers and invoking callbacks.
	 * @param command_buffer A begun VkCommandBuffer to record into.
	 * @return RenderError::None on success.
	 */
	[[nodiscard]] RenderError Execute(VkCommandBuffer command_buffer);

	/**
	 * @brief End the current frame, releasing declared handles back to their registries.
	 */
	void End();

	/**
	 * @brief Return the number of passes in the current frame.
	 * @return Pass count.
	 */
	[[nodiscard]] uint32_t GetPassCount() const noexcept;

	/**
	 * @brief Return whether this Graph has been successfully initialized.
	 * @return true if initialized and not moved-from.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

	private:
	/// Represents an imported (externally-owned) image entry.
	struct ImportedImageEntry
	{
		VkImage image = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkExtent2D extent = {0, 0};
		ResourceUsage currentUsage = ResourceUsage::Undefined;
	};

	/// Helper: get the (stage, access, layout) triple for a ResourceUsage.
	struct UsageTriple
	{
		VkPipelineStageFlags2 stage;
		VkAccessFlags2 access;
		VkImageLayout layout;
	};

	static UsageTriple GetUsageTriple(ResourceUsage usage);

	/// Resolve image view for a handle (import table or registry).
	VkImageView ResolveImageView(ImageHandle handle) const;
	/// Resolve VkImage for a handle.
	VkImage ResolveImage(ImageHandle handle) const;
	/// Resolve VkExtent2D for an image handle.
	VkExtent2D ResolveImageExtent(ImageHandle handle) const;
	/// Get current cached usage for an image handle.
	ResourceUsage GetImageCurrentUsage(ImageHandle handle) const;
	/// Set cached usage for an image handle.
	void SetImageCurrentUsage(ImageHandle handle, ResourceUsage usage);
	/// Get current cached usage for a buffer handle.
	ResourceUsage GetBufferCurrentUsage(BufferHandle handle) const;
	/// Set cached usage for a buffer handle.
	void SetBufferCurrentUsage(BufferHandle handle, ResourceUsage usage);

	/// Clear all per-frame state without releasing declared handles to registries.
	void ClearPerFrameState();

	/// Whether the handle refers to an imported image (high bit set).
	static bool IsImportedHandle(ImageHandle handle);
	static uint32_t ImportIndex(ImageHandle handle);

	const Device* _device = nullptr;
	ImageRegistry* _imageRegistry = nullptr;
	BufferRegistry* _bufferRegistry = nullptr;

	bool _initialized = false;

	/// Per-frame pass list.
	std::vector<Pass> _passes;

	/// Bump arena for PassBuilder objects (one per AddPass call).
	std::vector<PassBuilder*> _passBuilderArena;

	/// Per-frame import table.
	std::vector<ImportedImageEntry> _importTable;

	/// Per-frame declared image handles (for End to release).
	std::vector<ImageHandle> _declaredImages;

	/// Per-frame declared buffer handles (for End to release).
	std::vector<BufferHandle> _declaredBuffers;
};

} // namespace virasa::renderer::graph

#endif // VIRASA_RENDERER_GRAPH_GRAPH_H
