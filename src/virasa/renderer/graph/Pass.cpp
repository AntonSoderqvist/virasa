#include "virasa/renderer/graph/Pass.h"

#include <utility>

#include "virasa/renderer/resources/Buffer.h"
#include "virasa/renderer/resources/Image.h"

namespace virasa::renderer::graph
{
GraphContext::GraphContext(VkCommandBuffer command_buffer, VkExtent2D render_extent,
	const ImageRegistry& image_registry, const BufferRegistry& buffer_registry,
	const Device& device) noexcept
    : _commandBuffer(command_buffer), _renderExtent(render_extent), _imageRegistry(&image_registry),
	_bufferRegistry(&buffer_registry), _device(&device)
{
}

VkCommandBuffer GraphContext::GetCommandBuffer() const noexcept
{
	return _commandBuffer;
}

VkExtent2D GraphContext::GetRenderExtent() const noexcept
{
	return _renderExtent;
}

VkImageView GraphContext::GetImageView(ImageHandle handle) const noexcept
{
	return _imageRegistry->Get(handle).GetView();
}

VkImage GraphContext::GetImage(ImageHandle handle) const noexcept
{
	return _imageRegistry->Get(handle).GetHandle();
}

VkBuffer GraphContext::GetBuffer(BufferHandle handle) const noexcept
{
	return _bufferRegistry->Get(handle).GetHandle();
}

VkDeviceAddress GraphContext::GetBufferAddress(BufferHandle handle) const noexcept
{
	return _bufferRegistry->Get(handle).GetDeviceAddress(*_device);
}

Pass::Pass(Pass&& other) noexcept
    : _name(other._name), _colorAttachments(std::move(other._colorAttachments)),
	_depthAttachment(other._depthAttachment), _imageReads(std::move(other._imageReads)),
	_imageWrites(std::move(other._imageWrites)), _imageAccesses(std::move(other._imageAccesses)),
	_bufferReads(std::move(other._bufferReads)), _bufferWrites(std::move(other._bufferWrites)),
	_bufferAccesses(std::move(other._bufferAccesses)),
	_recordCallback(std::move(other._recordCallback))
{
	other.ResetMovedFrom();
}

Pass& Pass::operator=(Pass&& other) noexcept
{
	if (this != &other)
	{
		_name = other._name;
		_colorAttachments = std::move(other._colorAttachments);
		_depthAttachment = other._depthAttachment;
		_imageReads = std::move(other._imageReads);
		_imageWrites = std::move(other._imageWrites);
		_imageAccesses = std::move(other._imageAccesses);
		_bufferReads = std::move(other._bufferReads);
		_bufferWrites = std::move(other._bufferWrites);
		_bufferAccesses = std::move(other._bufferAccesses);
		_recordCallback = std::move(other._recordCallback);
		other.ResetMovedFrom();
	}

	return *this;
}

const char* Pass::GetName() const noexcept
{
	return _name;
}

std::span<const ColorAttachmentInfo> Pass::GetColorAttachments() const noexcept
{
	return std::span<const ColorAttachmentInfo>(
		_colorAttachments.data(), _colorAttachments.size());
}

bool Pass::HasDepthAttachment() const noexcept
{
	return _depthAttachment.present;
}

const DepthAttachmentInfo& Pass::GetDepthAttachment() const noexcept
{
	return _depthAttachment;
}

std::span<const ImageAccess> Pass::GetImageAccesses() const noexcept
{
	return std::span<const ImageAccess>(_imageAccesses.data(), _imageAccesses.size());
}

std::span<const BufferAccess> Pass::GetBufferAccesses() const noexcept
{
	return std::span<const BufferAccess>(_bufferAccesses.data(), _bufferAccesses.size());
}

bool Pass::HasRecordCallback() const noexcept
{
	return static_cast<bool>(_recordCallback);
}

void Pass::Invoke(const GraphContext& ctx) const
{
	_recordCallback(ctx);
}

void Pass::RebuildImageAccesses()
{
	_imageAccesses.clear();
	_imageAccesses.reserve(_imageReads.size() + _imageWrites.size());
	_imageAccesses.insert(_imageAccesses.end(), _imageReads.begin(), _imageReads.end());
	_imageAccesses.insert(_imageAccesses.end(), _imageWrites.begin(), _imageWrites.end());
}

void Pass::RebuildBufferAccesses()
{
	_bufferAccesses.clear();
	_bufferAccesses.reserve(_bufferReads.size() + _bufferWrites.size());
	_bufferAccesses.insert(_bufferAccesses.end(), _bufferReads.begin(), _bufferReads.end());
	_bufferAccesses.insert(_bufferAccesses.end(), _bufferWrites.begin(), _bufferWrites.end());
}

void Pass::ResetMovedFrom() noexcept
{
	_name = nullptr;
	_colorAttachments.clear();
	_depthAttachment = {};
	_imageReads.clear();
	_imageWrites.clear();
	_imageAccesses.clear();
	_bufferReads.clear();
	_bufferWrites.clear();
	_bufferAccesses.clear();
	_recordCallback = {};
}

PassBuilder::PassBuilder(Pass& pass, const char* name) noexcept : _pass(&pass) { pass._name = name; }

PassBuilder& PassBuilder::ColorAttachment(ImageHandle image, LoadOp loadOp, ClearColor clearColor)
{
	_pass->_colorAttachments.push_back(ColorAttachmentInfo{
		.image = image,
		.loadOp = loadOp,
		.storeOp = StoreOp::Store,
		.clearColor = clearColor,
	});
	return *this;
}

PassBuilder& PassBuilder::DepthAttachment(ImageHandle image, LoadOp loadOp, float clearDepth)
{
	_pass->_depthAttachment = DepthAttachmentInfo{
		.image = image,
		.loadOp = loadOp,
		.storeOp = StoreOp::DontCare,
		.clearDepth = clearDepth,
		.present = true,
	};
	return *this;
}

PassBuilder& PassBuilder::Read(ImageHandle image, ResourceUsage usage)
{
	_pass->_imageReads.push_back(ImageAccess{.image = image, .usage = usage});
	_pass->RebuildImageAccesses();
	return *this;
}

PassBuilder& PassBuilder::Read(BufferHandle buffer, ResourceUsage usage)
{
	_pass->_bufferReads.push_back(BufferAccess{.buffer = buffer, .usage = usage});
	_pass->RebuildBufferAccesses();
	return *this;
}

PassBuilder& PassBuilder::Write(ImageHandle image, ResourceUsage usage)
{
	_pass->_imageWrites.push_back(ImageAccess{.image = image, .usage = usage});
	_pass->RebuildImageAccesses();
	return *this;
}

PassBuilder& PassBuilder::Write(BufferHandle buffer, ResourceUsage usage)
{
	_pass->_bufferWrites.push_back(BufferAccess{.buffer = buffer, .usage = usage});
	_pass->RebuildBufferAccesses();
	return *this;
}

PassBuilder& PassBuilder::Record(std::function<void(const GraphContext&)> callback)
{
	_pass->_recordCallback = std::move(callback);
	return *this;
}
} // namespace virasa::renderer::graph
