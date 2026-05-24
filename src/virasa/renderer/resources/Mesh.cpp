#include "virasa/renderer/resources/Mesh.h"

#include "quill/LogMacros.h"

#include "virasa/core/Logger.h"

namespace virasa
{

RenderError Mesh::Initialize(
	const Device& device, const Context& context, const MeshData& mesh_data)
{
	_vertexBuffer = Buffer{};
	_indexBuffer = Buffer{};
	_indexCount = 0;

	BufferConfig vertexConfig{};
	vertexConfig.size = static_cast<VkDeviceSize>(mesh_data.vertices.size() * sizeof(Vertex));
	vertexConfig.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
				   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	vertexConfig.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	RenderError error = _vertexBuffer.Initialize(device, vertexConfig);
	if (error != RenderError::None)
	{
		LOG_ERROR(Logger::GetLogger("renderer"),
			"Failed to initialize mesh vertex buffer: {}",
			static_cast<uint8_t>(error));
		_vertexBuffer = Buffer{};
		_indexBuffer = Buffer{};
		_indexCount = 0;
		return error;
	}

	error = _vertexBuffer.UploadViaStaging(device,
		context.GetTransferCommandPool(),
		device.GetTransferQueue(),
		mesh_data.vertices.data(),
		mesh_data.vertices.size() * sizeof(Vertex));
	if (error != RenderError::None)
	{
		LOG_ERROR(
			Logger::GetLogger("renderer"), "Failed to upload mesh vertex buffer: {}", static_cast<uint8_t>(error));
		_vertexBuffer = Buffer{};
		_indexBuffer = Buffer{};
		_indexCount = 0;
		return error;
	}

	BufferConfig indexConfig{};
	indexConfig.size = static_cast<VkDeviceSize>(mesh_data.indices.size() * sizeof(uint32_t));
	indexConfig.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
				  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
				  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	indexConfig.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	error = _indexBuffer.Initialize(device, indexConfig);
	if (error != RenderError::None)
	{
		LOG_ERROR(Logger::GetLogger("renderer"),
			"Failed to initialize mesh index buffer: {}",
			static_cast<uint8_t>(error));
		_vertexBuffer = Buffer{};
		_indexBuffer = Buffer{};
		_indexCount = 0;
		return error;
	}

	error = _indexBuffer.UploadViaStaging(device,
		context.GetTransferCommandPool(),
		device.GetTransferQueue(),
		mesh_data.indices.data(),
		mesh_data.indices.size() * sizeof(uint32_t));
	if (error != RenderError::None)
	{
		LOG_ERROR(
			Logger::GetLogger("renderer"), "Failed to upload mesh index buffer: {}", static_cast<uint8_t>(error));
		_vertexBuffer = Buffer{};
		_indexBuffer = Buffer{};
		_indexCount = 0;
		return error;
	}

	_indexCount = static_cast<uint32_t>(mesh_data.indices.size());
	return RenderError::None;
}

VkDeviceAddress Mesh::GetVertexBufferAddress(const Device& device) const noexcept
{
	return _vertexBuffer.GetDeviceAddress(device);
}

VkDeviceAddress Mesh::GetIndexBufferAddress(const Device& device) const noexcept
{
	return _indexBuffer.GetDeviceAddress(device);
}

uint32_t Mesh::GetIndexCount() const noexcept
{
	return _indexCount;
}

bool Mesh::IsInitialized() const noexcept
{
	return _vertexBuffer.IsInitialized() && _indexBuffer.IsInitialized();
}

} // namespace virasa
