#include "virasa/renderer/graph/Graph.h"

#include <cassert>
#include <cstring>
#include <vector>

#include "quill/LogMacros.h"
#include "virasa/core/Logger.h"
#include "virasa/renderer/core/Device.h"
#include "vulkan/vulkan.h"

namespace virasa::renderer::graph
{

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

static constexpr uint32_t kImportBit = 0x80000000u;

bool Graph::IsImportedHandle(ImageHandle handle)
{
	return (handle.id & kImportBit) != 0u;
}

uint32_t Graph::ImportIndex(ImageHandle handle)
{
	return handle.id & ~kImportBit;
}

Graph::UsageTriple Graph::GetUsageTriple(ResourceUsage usage)
{
	switch (usage)
	{
		case ResourceUsage::Undefined:
			return {VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
				VK_ACCESS_2_NONE,
				VK_IMAGE_LAYOUT_UNDEFINED};
		case ResourceUsage::ColorAttachment:
			return {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
		case ResourceUsage::DepthAttachment:
			return {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
					  VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
					VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
				VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL};
		case ResourceUsage::DepthReadOnly:
			return {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
					  VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT |
					  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
				VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL};
		case ResourceUsage::SampledFragment:
			return {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
		case ResourceUsage::SampledCompute:
			return {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
		case ResourceUsage::StorageReadCompute:
			return {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL};
		case ResourceUsage::StorageWriteCompute:
			return {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
				VK_IMAGE_LAYOUT_GENERAL};
		case ResourceUsage::TransferSrc:
			return {VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
				VK_ACCESS_2_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
		case ResourceUsage::TransferDst:
			return {VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
				VK_ACCESS_2_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL};
		case ResourceUsage::Present:
			return {VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
				VK_ACCESS_2_NONE,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
		default:
			return {VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
				VK_ACCESS_2_NONE,
				VK_IMAGE_LAYOUT_UNDEFINED};
	}
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

Graph::Graph(Graph&& other) noexcept
    : _device(other._device), _imageRegistry(other._imageRegistry),
	_bufferRegistry(other._bufferRegistry), _initialized(other._initialized),
	_passes(std::move(other._passes)), _passBuilderArena(std::move(other._passBuilderArena)),
	_importTable(std::move(other._importTable)),
	_declaredImages(std::move(other._declaredImages)),
	_declaredBuffers(std::move(other._declaredBuffers))
{
	other._device = nullptr;
	other._imageRegistry = nullptr;
	other._bufferRegistry = nullptr;
	other._initialized = false;
}

Graph& Graph::operator=(Graph&& other) noexcept
{
	if (this == &other)
		return *this;

	// Clear any retained per-frame state on the destination (equivalent to End on in-progress
	// frame, but without releasing declared handles — we just clear them to avoid double-free).
	for (auto* pb : _passBuilderArena)
		delete pb;
	_passBuilderArena.clear();
	_passes.clear();
	_importTable.clear();
	_declaredImages.clear();
	_declaredBuffers.clear();

	_device = other._device;
	_imageRegistry = other._imageRegistry;
	_bufferRegistry = other._bufferRegistry;
	_initialized = other._initialized;
	_passes = std::move(other._passes);
	_passBuilderArena = std::move(other._passBuilderArena);
	_importTable = std::move(other._importTable);
	_declaredImages = std::move(other._declaredImages);
	_declaredBuffers = std::move(other._declaredBuffers);

	other._device = nullptr;
	other._imageRegistry = nullptr;
	other._bufferRegistry = nullptr;
	other._initialized = false;

	return *this;
}

Graph::~Graph()
{
	for (auto* pb : _passBuilderArena)
		delete pb;
	_passBuilderArena.clear();
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

RenderError Graph::Initialize(
	const Device& device, ImageRegistry& image_registry, BufferRegistry& buffer_registry)
{
	if (_initialized)
	{
		// Re-initialization: clear retained per-frame state.
		for (auto* pb : _passBuilderArena)
			delete pb;
		_passBuilderArena.clear();
		_passes.clear();
		_importTable.clear();
		_declaredImages.clear();
		_declaredBuffers.clear();
		_initialized = false;
		_device = nullptr;
		_imageRegistry = nullptr;
		_bufferRegistry = nullptr;
	}

	_device = &device;
	_imageRegistry = &image_registry;
	_bufferRegistry = &buffer_registry;
	_initialized = true;

	return RenderError::None;
}

// ---------------------------------------------------------------------------
// Per-frame lifecycle
// ---------------------------------------------------------------------------

void Graph::Begin()
{
	// Clear per-frame state. Declared handles are NOT released here (that's End's job).
	// Calling Begin mid-frame without End leaks declared handles — per contract.
	for (auto* pb : _passBuilderArena)
		delete pb;
	_passBuilderArena.clear();
	_passes.clear();
	_importTable.clear();
	_declaredImages.clear();
	_declaredBuffers.clear();
}

void Graph::End()
{
	// Release declared handles in LIFO order.
	for (int32_t i = static_cast<int32_t>(_declaredImages.size()) - 1; i >= 0; --i)
		_imageRegistry->Free(_declaredImages[static_cast<size_t>(i)]);
	for (int32_t i = static_cast<int32_t>(_declaredBuffers.size()) - 1; i >= 0; --i)
		_bufferRegistry->Free(_declaredBuffers[static_cast<size_t>(i)]);

	_declaredImages.clear();
	_declaredBuffers.clear();
	_importTable.clear();

	for (auto* pb : _passBuilderArena)
		delete pb;
	_passBuilderArena.clear();
	_passes.clear();
}

// ---------------------------------------------------------------------------
// Resource declaration
// ---------------------------------------------------------------------------

ImageHandle Graph::ImportImage(VkImage image, VkImageView view, VkFormat format, VkExtent2D extent,
	ResourceUsage current_usage)
{
	ImportedImageEntry entry;
	entry.image = image;
	entry.view = view;
	entry.format = format;
	entry.extent = extent;
	entry.currentUsage = current_usage;

	uint32_t index = static_cast<uint32_t>(_importTable.size());
	_importTable.push_back(entry);

	ImageHandle handle;
	handle.id = index | kImportBit;
	return handle;
}

ImageHandle Graph::DeclareImage(const GraphImageDesc& desc)
{
	ImageHandle handle = _imageRegistry->Allocate(desc);
	if (!handle.IsValid())
	{
		auto* logger = Logger::GetLogger("renderer");
		LOG_ERROR(logger, "Graph::DeclareImage: ImageRegistry::Allocate failed");
		return handle;
	}
	_declaredImages.push_back(handle);
	return handle;
}

BufferHandle Graph::DeclareBuffer(const GraphBufferDesc& desc)
{
	BufferHandle handle = _bufferRegistry->Allocate(desc);
	if (!handle.IsValid())
	{
		auto* logger = Logger::GetLogger("renderer");
		LOG_ERROR(logger, "Graph::DeclareBuffer: BufferRegistry::Allocate failed");
		return handle;
	}
	_declaredBuffers.push_back(handle);
	return handle;
}

PassBuilder& Graph::AddPass(const char* name)
{
	_passes.emplace_back();
	Pass& pass = _passes.back();

	// Construct a PassBuilder in the bump arena.
	PassBuilder* builder = new PassBuilder(pass, name);
	_passBuilderArena.push_back(builder);
	return *builder;
}

// ---------------------------------------------------------------------------
// Compile
// ---------------------------------------------------------------------------

RenderError Graph::Compile()
{
	auto* logger = Logger::GetLogger("renderer");

	for (const Pass& pass : _passes)
	{
		// Validate record callback.
		if (!pass.HasRecordCallback())
		{
			LOG_ERROR(logger,
				"Graph::Compile: pass '{}' has no record callback",
				pass.GetName() ? pass.GetName() : "<unnamed>");
			return RenderError::PipelineCreateFailed;
		}

		// Validate color attachments.
		for (const ColorAttachmentInfo& ca : pass.GetColorAttachments())
		{
			if (!ca.image.IsValid())
			{
				LOG_ERROR(logger,
					"Graph::Compile: pass '{}' has invalid color attachment handle",
					pass.GetName() ? pass.GetName() : "<unnamed>");
				return RenderError::PipelineCreateFailed;
			}
			if (!IsImportedHandle(ca.image))
			{
				if (!_imageRegistry->IsAllocated(ca.image))
				{
					LOG_ERROR(logger,
						"Graph::Compile: pass '{}' color attachment slot not allocated",
						pass.GetName() ? pass.GetName() : "<unnamed>");
					return RenderError::PipelineCreateFailed;
				}
			}
			else
			{
				if (ImportIndex(ca.image) >= static_cast<uint32_t>(_importTable.size()))
				{
					LOG_ERROR(logger,
						"Graph::Compile: pass '{}' color attachment import index out "
						"of range",
						pass.GetName() ? pass.GetName() : "<unnamed>");
					return RenderError::PipelineCreateFailed;
				}
			}
		}

		// Validate depth attachment.
		if (pass.HasDepthAttachment())
		{
			const DepthAttachmentInfo& da = pass.GetDepthAttachment();
			if (!da.image.IsValid())
			{
				LOG_ERROR(logger,
					"Graph::Compile: pass '{}' has invalid depth attachment handle",
					pass.GetName() ? pass.GetName() : "<unnamed>");
				return RenderError::PipelineCreateFailed;
			}
			if (!IsImportedHandle(da.image))
			{
				if (!_imageRegistry->IsAllocated(da.image))
				{
					LOG_ERROR(logger,
						"Graph::Compile: pass '{}' depth attachment slot not allocated",
						pass.GetName() ? pass.GetName() : "<unnamed>");
					return RenderError::PipelineCreateFailed;
				}
			}
			else
			{
				if (ImportIndex(da.image) >= static_cast<uint32_t>(_importTable.size()))
				{
					LOG_ERROR(logger,
						"Graph::Compile: pass '{}' depth attachment import index out "
						"of range",
						pass.GetName() ? pass.GetName() : "<unnamed>");
					return RenderError::PipelineCreateFailed;
				}
			}
		}

		// Validate image accesses.
		for (const ImageAccess& ia : pass.GetImageAccesses())
		{
			if (!ia.image.IsValid())
			{
				LOG_ERROR(logger,
					"Graph::Compile: pass '{}' has invalid image access handle",
					pass.GetName() ? pass.GetName() : "<unnamed>");
				return RenderError::PipelineCreateFailed;
			}
			if (!IsImportedHandle(ia.image))
			{
				if (!_imageRegistry->IsAllocated(ia.image))
				{
					LOG_ERROR(logger,
						"Graph::Compile: pass '{}' image access slot not allocated",
						pass.GetName() ? pass.GetName() : "<unnamed>");
					return RenderError::PipelineCreateFailed;
				}
			}
			else
			{
				if (ImportIndex(ia.image) >= static_cast<uint32_t>(_importTable.size()))
				{
					LOG_ERROR(logger,
						"Graph::Compile: pass '{}' image access import index out of "
						"range",
						pass.GetName() ? pass.GetName() : "<unnamed>");
					return RenderError::PipelineCreateFailed;
				}
			}
		}

		// Validate buffer accesses.
		for (const BufferAccess& ba : pass.GetBufferAccesses())
		{
			if (!ba.buffer.IsValid())
			{
				LOG_ERROR(logger,
					"Graph::Compile: pass '{}' has invalid buffer access handle",
					pass.GetName() ? pass.GetName() : "<unnamed>");
				return RenderError::PipelineCreateFailed;
			}
			if (!_bufferRegistry->IsAllocated(ba.buffer))
			{
				LOG_ERROR(logger,
					"Graph::Compile: pass '{}' buffer access slot not allocated",
					pass.GetName() ? pass.GetName() : "<unnamed>");
				return RenderError::PipelineCreateFailed;
			}
		}
	}

	return RenderError::None;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

VkImageView Graph::ResolveImageView(ImageHandle handle) const
{
	if (IsImportedHandle(handle))
		return _importTable[ImportIndex(handle)].view;
	return _imageRegistry->Get(handle).GetView();
}

VkImage Graph::ResolveImage(ImageHandle handle) const
{
	if (IsImportedHandle(handle))
		return _importTable[ImportIndex(handle)].image;
	return _imageRegistry->Get(handle).GetHandle();
}

VkExtent2D Graph::ResolveImageExtent(ImageHandle handle) const
{
	if (IsImportedHandle(handle))
		return _importTable[ImportIndex(handle)].extent;
	return _imageRegistry->Get(handle).GetExtent();
}

ResourceUsage Graph::GetImageCurrentUsage(ImageHandle handle) const
{
	if (IsImportedHandle(handle))
		return _importTable[ImportIndex(handle)].currentUsage;
	return _imageRegistry->GetUsage(handle);
}

void Graph::SetImageCurrentUsage(ImageHandle handle, ResourceUsage usage)
{
	if (IsImportedHandle(handle))
		_importTable[ImportIndex(handle)].currentUsage = usage;
	else
		_imageRegistry->SetUsage(handle, usage);
}

ResourceUsage Graph::GetBufferCurrentUsage(BufferHandle handle) const
{
	return _bufferRegistry->GetUsage(handle);
}

void Graph::SetBufferCurrentUsage(BufferHandle handle, ResourceUsage usage)
{
	_bufferRegistry->SetUsage(handle, usage);
}

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------

RenderError Graph::Execute(VkCommandBuffer command_buffer)
{
	for (const Pass& pass : _passes)
	{
		// ---------------------------------------------------------------
		// Step 1 & 2: gather transitions and emit pre-pass barrier
		// ---------------------------------------------------------------
		std::vector<VkImageMemoryBarrier2> imageBarriers;
		std::vector<VkBufferMemoryBarrier2> bufferBarriers;

		// Color attachments → ColorAttachment usage
		for (const ColorAttachmentInfo& ca : pass.GetColorAttachments())
		{
			ResourceUsage fromUsage = GetImageCurrentUsage(ca.image);
			ResourceUsage toUsage = ResourceUsage::ColorAttachment;

			UsageTriple fromTriple = GetUsageTriple(fromUsage);
			UsageTriple toTriple = GetUsageTriple(toUsage);

			if (fromTriple.stage != toTriple.stage || fromTriple.access != toTriple.access ||
				fromTriple.layout != toTriple.layout)
			{
				VkImageMemoryBarrier2 barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
				barrier.srcStageMask = fromTriple.stage;
				barrier.srcAccessMask = fromTriple.access;
				barrier.dstStageMask = toTriple.stage;
				barrier.dstAccessMask = toTriple.access;
				barrier.oldLayout = fromTriple.layout;
				barrier.newLayout = toTriple.layout;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = ResolveImage(ca.image);
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
				imageBarriers.push_back(barrier);
			}
		}

		// Depth attachment → DepthAttachment usage
		if (pass.HasDepthAttachment())
		{
			const DepthAttachmentInfo& da = pass.GetDepthAttachment();
			ResourceUsage fromUsage = GetImageCurrentUsage(da.image);
			ResourceUsage toUsage = ResourceUsage::DepthAttachment;

			UsageTriple fromTriple = GetUsageTriple(fromUsage);
			UsageTriple toTriple = GetUsageTriple(toUsage);

			if (fromTriple.stage != toTriple.stage || fromTriple.access != toTriple.access ||
				fromTriple.layout != toTriple.layout)
			{
				// Determine aspect from registry or import table.
				VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
				if (!IsImportedHandle(da.image))
					aspect = _imageRegistry->GetDesc(da.image).aspect;

				VkImageMemoryBarrier2 barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
				barrier.srcStageMask = fromTriple.stage;
				barrier.srcAccessMask = fromTriple.access;
				barrier.dstStageMask = toTriple.stage;
				barrier.dstAccessMask = toTriple.access;
				barrier.oldLayout = fromTriple.layout;
				barrier.newLayout = toTriple.layout;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = ResolveImage(da.image);
				barrier.subresourceRange.aspectMask = aspect;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
				imageBarriers.push_back(barrier);
			}
		}

		// Image accesses (reads + writes)
		for (const ImageAccess& ia : pass.GetImageAccesses())
		{
			ResourceUsage fromUsage = GetImageCurrentUsage(ia.image);
			ResourceUsage toUsage = ia.usage;

			UsageTriple fromTriple = GetUsageTriple(fromUsage);
			UsageTriple toTriple = GetUsageTriple(toUsage);

			if (fromTriple.stage != toTriple.stage || fromTriple.access != toTriple.access ||
				fromTriple.layout != toTriple.layout)
			{
				VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
				if (!IsImportedHandle(ia.image))
					aspect = _imageRegistry->GetDesc(ia.image).aspect;

				VkImageMemoryBarrier2 barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
				barrier.srcStageMask = fromTriple.stage;
				barrier.srcAccessMask = fromTriple.access;
				barrier.dstStageMask = toTriple.stage;
				barrier.dstAccessMask = toTriple.access;
				barrier.oldLayout = fromTriple.layout;
				barrier.newLayout = toTriple.layout;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = ResolveImage(ia.image);
				barrier.subresourceRange.aspectMask = aspect;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
				imageBarriers.push_back(barrier);
			}
		}

		// Buffer accesses (reads + writes)
		for (const BufferAccess& ba : pass.GetBufferAccesses())
		{
			ResourceUsage fromUsage = GetBufferCurrentUsage(ba.buffer);
			ResourceUsage toUsage = ba.usage;

			UsageTriple fromTriple = GetUsageTriple(fromUsage);
			UsageTriple toTriple = GetUsageTriple(toUsage);

			if (fromTriple.stage != toTriple.stage || fromTriple.access != toTriple.access)
			{
				VkBufferMemoryBarrier2 barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
				barrier.srcStageMask = fromTriple.stage;
				barrier.srcAccessMask = fromTriple.access;
				barrier.dstStageMask = toTriple.stage;
				barrier.dstAccessMask = toTriple.access;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.buffer = _bufferRegistry->Get(ba.buffer).GetHandle();
				barrier.offset = 0;
				barrier.size = VK_WHOLE_SIZE;
				bufferBarriers.push_back(barrier);
			}
		}

		// Emit the pre-pass barrier if there's anything to emit.
		if (!imageBarriers.empty() || !bufferBarriers.empty())
		{
			VkDependencyInfo depInfo{};
			depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			depInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size());
			depInfo.pImageMemoryBarriers = imageBarriers.data();
			depInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size());
			depInfo.pBufferMemoryBarriers = bufferBarriers.data();
			vkCmdPipelineBarrier2(command_buffer, &depInfo);
		}

		// ---------------------------------------------------------------
		// Step 3: update cached usage (before callback runs)
		// ---------------------------------------------------------------
		for (const ColorAttachmentInfo& ca : pass.GetColorAttachments())
			SetImageCurrentUsage(ca.image, ResourceUsage::ColorAttachment);

		if (pass.HasDepthAttachment())
			SetImageCurrentUsage(
				pass.GetDepthAttachment().image, ResourceUsage::DepthAttachment);

		for (const ImageAccess& ia : pass.GetImageAccesses())
			SetImageCurrentUsage(ia.image, ia.usage);

		for (const BufferAccess& ba : pass.GetBufferAccesses())
			SetBufferCurrentUsage(ba.buffer, ba.usage);

		// ---------------------------------------------------------------
		// Step 4: begin rendering (if attachment pass)
		// ---------------------------------------------------------------
		bool hasAttachments = !pass.GetColorAttachments().empty() || pass.HasDepthAttachment();
		VkExtent2D renderExtent = {0, 0};

		if (hasAttachments)
		{
			// Determine render extent.
			if (!pass.GetColorAttachments().empty())
				renderExtent = ResolveImageExtent(pass.GetColorAttachments()[0].image);
			else if (pass.HasDepthAttachment())
				renderExtent = ResolveImageExtent(pass.GetDepthAttachment().image);

			// Build color attachment infos.
			std::vector<VkRenderingAttachmentInfo> colorAttachInfos;
			colorAttachInfos.reserve(pass.GetColorAttachments().size());

			for (const ColorAttachmentInfo& ca : pass.GetColorAttachments())
			{
				VkRenderingAttachmentInfo ai{};
				ai.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				ai.imageView = ResolveImageView(ca.image);
				ai.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				ai.loadOp = (ca.loadOp == LoadOp::Load) ? VK_ATTACHMENT_LOAD_OP_LOAD
						: (ca.loadOp == LoadOp::Clear)
							? VK_ATTACHMENT_LOAD_OP_CLEAR
							: VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				ai.storeOp = (ca.storeOp == StoreOp::Store)
							 ? VK_ATTACHMENT_STORE_OP_STORE
							 : VK_ATTACHMENT_STORE_OP_DONT_CARE;
				ai.clearValue.color.float32[0] = ca.clearColor.r;
				ai.clearValue.color.float32[1] = ca.clearColor.g;
				ai.clearValue.color.float32[2] = ca.clearColor.b;
				ai.clearValue.color.float32[3] = ca.clearColor.a;
				colorAttachInfos.push_back(ai);
			}

			// Build depth attachment info.
			VkRenderingAttachmentInfo depthAttachInfo{};
			VkRenderingAttachmentInfo* pDepthAttach = nullptr;

			if (pass.HasDepthAttachment())
			{
				const DepthAttachmentInfo& da = pass.GetDepthAttachment();
				depthAttachInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				depthAttachInfo.imageView = ResolveImageView(da.image);
				depthAttachInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
				depthAttachInfo.loadOp =
					(da.loadOp == LoadOp::Load)	 ? VK_ATTACHMENT_LOAD_OP_LOAD
					: (da.loadOp == LoadOp::Clear) ? VK_ATTACHMENT_LOAD_OP_CLEAR
										 : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				depthAttachInfo.storeOp = (da.storeOp == StoreOp::Store)
									  ? VK_ATTACHMENT_STORE_OP_STORE
									  : VK_ATTACHMENT_STORE_OP_DONT_CARE;
				depthAttachInfo.clearValue.depthStencil.depth = da.clearDepth;
				depthAttachInfo.clearValue.depthStencil.stencil = 0;
				pDepthAttach = &depthAttachInfo;
			}

			VkRenderingInfo renderingInfo{};
			renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			renderingInfo.renderArea.offset = {0, 0};
			renderingInfo.renderArea.extent = renderExtent;
			renderingInfo.layerCount = 1;
			renderingInfo.viewMask = 0;
			renderingInfo.colorAttachmentCount =
				static_cast<uint32_t>(colorAttachInfos.size());
			renderingInfo.pColorAttachments =
				colorAttachInfos.empty() ? nullptr : colorAttachInfos.data();
			renderingInfo.pDepthAttachment = pDepthAttach;
			renderingInfo.pStencilAttachment = nullptr;

			vkCmdBeginRendering(command_buffer, &renderingInfo);
		}

		// ---------------------------------------------------------------
		// Step 5: invoke record callback
		// ---------------------------------------------------------------
		GraphContext ctx(
			command_buffer, renderExtent, *_imageRegistry, *_bufferRegistry, *_device);
		pass.Invoke(ctx);

		// ---------------------------------------------------------------
		// Step 6: end rendering
		// ---------------------------------------------------------------
		if (hasAttachments)
			vkCmdEndRendering(command_buffer);
	}

	// ---------------------------------------------------------------
	// Automatic present transition for imported color images
	// ---------------------------------------------------------------
	std::vector<VkImageMemoryBarrier2> presentBarriers;

	for (uint32_t i = 0; i < static_cast<uint32_t>(_importTable.size()); ++i)
	{
		ImportedImageEntry& entry = _importTable[i];
		if (entry.currentUsage == ResourceUsage::ColorAttachment)
		{
			UsageTriple fromTriple = GetUsageTriple(ResourceUsage::ColorAttachment);
			UsageTriple toTriple = GetUsageTriple(ResourceUsage::Present);

			VkImageMemoryBarrier2 barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			barrier.srcStageMask = fromTriple.stage;
			barrier.srcAccessMask = fromTriple.access;
			barrier.dstStageMask = toTriple.stage;
			barrier.dstAccessMask = toTriple.access;
			barrier.oldLayout = fromTriple.layout;
			barrier.newLayout = toTriple.layout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = entry.image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
			presentBarriers.push_back(barrier);

			entry.currentUsage = ResourceUsage::Present;
		}
	}

	if (!presentBarriers.empty())
	{
		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.imageMemoryBarrierCount = static_cast<uint32_t>(presentBarriers.size());
		depInfo.pImageMemoryBarriers = presentBarriers.data();
		vkCmdPipelineBarrier2(command_buffer, &depInfo);
	}

	return RenderError::None;
}

// ---------------------------------------------------------------------------
// Observers
// ---------------------------------------------------------------------------

uint32_t Graph::GetPassCount() const noexcept
{
	return static_cast<uint32_t>(_passes.size());
}

bool Graph::IsInitialized() const noexcept
{
	return _initialized;
}

} // namespace virasa::renderer::graph
