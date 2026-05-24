#include "virasa/renderer/resources/Buffer.h"

#include <cassert>
#include <cstring>

#include "quill/LogMacros.h"
#include "virasa/core/Logger.h"

namespace virasa
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static constexpr uint32_t kInvalidMemoryType = UINT32_MAX;

// ---------------------------------------------------------------------------
// Special members
// ---------------------------------------------------------------------------

Buffer::Buffer(Buffer&& other) noexcept
    : _device(other._device), _buffer(other._buffer), _memory(other._memory), _size(other._size),
	_mapped(other._mapped)
{
	other._device = VK_NULL_HANDLE;
	other._buffer = VK_NULL_HANDLE;
	other._memory = VK_NULL_HANDLE;
	other._size = 0;
	other._mapped = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
	if (this != &other)
	{
		Teardown();

		_device = other._device;
		_buffer = other._buffer;
		_memory = other._memory;
		_size = other._size;
		_mapped = other._mapped;

		other._device = VK_NULL_HANDLE;
		other._buffer = VK_NULL_HANDLE;
		other._memory = VK_NULL_HANDLE;
		other._size = 0;
		other._mapped = nullptr;
	}
	return *this;
}

Buffer::~Buffer() noexcept
{
	Teardown();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void Buffer::Teardown() noexcept
{
	if (_device == VK_NULL_HANDLE)
	{
		return;
	}

	if (_mapped != nullptr)
	{
		vkUnmapMemory(_device, _memory);
		_mapped = nullptr;
	}

	if (_buffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(_device, _buffer, nullptr);
		_buffer = VK_NULL_HANDLE;
	}

	if (_memory != VK_NULL_HANDLE)
	{
		vkFreeMemory(_device, _memory, nullptr);
		_memory = VK_NULL_HANDLE;
	}

	_device = VK_NULL_HANDLE;
}

uint32_t Buffer::FindMemoryType(const VkPhysicalDeviceMemoryProperties& memProps,
	uint32_t typeFilter, VkMemoryPropertyFlags required) const noexcept
{
	for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
	{
		if ((typeFilter & (1u << i)) == 0)
		{
			continue;
		}
		if ((memProps.memoryTypes[i].propertyFlags & required) == required)
		{
			return i;
		}
	}
	return kInvalidMemoryType;
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

RenderError Buffer::Initialize(const Device& device, const BufferConfig& config)
{
	// Teardown any existing resources (re-initialization path).
	Teardown();

	auto* logger = Logger::GetLogger("renderer");

	if (config.size == 0)
	{
		LOG_ERROR(logger, "Buffer::Initialize — size must be greater than zero");
		return RenderError::BufferCreateFailed;
	}

	// Borrow the VkDevice and cache the size.
	_device = device.GetHandle();
	_size = config.size;

	// Step 1: create the VkBuffer.
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = config.size;
	bufferInfo.usage = config.usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result = vkCreateBuffer(_device, &bufferInfo, nullptr, &_buffer);
	if (result != VK_SUCCESS)
	{
		LOG_ERROR(logger,
			"Buffer::Initialize — vkCreateBuffer failed (VkResult {})",
			static_cast<int>(result));
		Teardown();
		return RenderError::BufferCreateFailed;
	}

	// Step 2: allocate backing memory.
	VkMemoryRequirements memReqs{};
	vkGetBufferMemoryRequirements(_device, _buffer, &memReqs);

	const VkPhysicalDeviceMemoryProperties& memProps = device.GetMemoryProperties();
	uint32_t memTypeIndex =
		FindMemoryType(memProps, memReqs.memoryTypeBits, config.memoryProperties);
	if (memTypeIndex == kInvalidMemoryType)
	{
		LOG_ERROR(logger,
			"Buffer::Initialize — no suitable memory type found for required flags 0x{:X}",
			static_cast<uint32_t>(config.memoryProperties));
		Teardown();
		return RenderError::MemoryAllocFailed;
	}

	VkMemoryAllocateFlagsInfo allocFlagsInfo{};
	allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = memTypeIndex;
	if (config.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
	{
		allocInfo.pNext = &allocFlagsInfo;
	}

	result = vkAllocateMemory(_device, &allocInfo, nullptr, &_memory);
	if (result != VK_SUCCESS)
	{
		LOG_ERROR(logger,
			"Buffer::Initialize — vkAllocateMemory failed (VkResult {})",
			static_cast<int>(result));
		Teardown();
		return RenderError::MemoryAllocFailed;
	}

	// Step 3: bind memory to buffer.
	result = vkBindBufferMemory(_device, _buffer, _memory, 0);
	if (result != VK_SUCCESS)
	{
		LOG_ERROR(logger,
			"Buffer::Initialize — vkBindBufferMemory failed (VkResult {})",
			static_cast<int>(result));
		Teardown();
		return RenderError::BufferCreateFailed;
	}

	return RenderError::None;
}

// ---------------------------------------------------------------------------
// Map / Unmap
// ---------------------------------------------------------------------------

void* Buffer::Map()
{
	if (_mapped != nullptr)
	{
		return _mapped;
	}

	void* ptr = nullptr;
	VkResult result = vkMapMemory(_device, _memory, 0, _size, 0, &ptr);
	if (result != VK_SUCCESS)
	{
		auto* logger = Logger::GetLogger("renderer");
		LOG_ERROR(logger,
			"Buffer::Map — vkMapMemory failed (VkResult {})",
			static_cast<int>(result));
		return nullptr;
	}

	_mapped = ptr;
	return _mapped;
}

void Buffer::Unmap()
{
	if (_mapped == nullptr)
	{
		return;
	}

	vkUnmapMemory(_device, _memory);
	_mapped = nullptr;
}

// ---------------------------------------------------------------------------
// Upload / Write
// ---------------------------------------------------------------------------

RenderError Buffer::Upload(const void* data, VkDeviceSize size_bytes)
{
	const bool wasAlreadyMapped = (_mapped != nullptr);

	if (!wasAlreadyMapped)
	{
		if (Map() == nullptr)
		{
			return RenderError::MemoryMapFailed;
		}
	}

	std::memcpy(_mapped, data, static_cast<size_t>(size_bytes));

	if (!wasAlreadyMapped)
	{
		Unmap();
	}

	return RenderError::None;
}

RenderError Buffer::Write(const void* data, size_t size, size_t offset)
{
	const bool wasAlreadyMapped = (_mapped != nullptr);

	if (!wasAlreadyMapped)
	{
		if (Map() == nullptr)
		{
			return RenderError::MemoryMapFailed;
		}
	}

	std::memcpy(static_cast<uint8_t*>(_mapped) + offset, data, size);

	if (!wasAlreadyMapped)
	{
		Unmap();
	}

	return RenderError::None;
}

// ---------------------------------------------------------------------------
// UploadViaStaging
// ---------------------------------------------------------------------------

RenderError Buffer::UploadViaStaging(const Device& device, VkCommandPool transfer_pool,
	VkQueue transfer_queue, const void* data, size_t size)
{
	auto* logger = Logger::GetLogger("renderer");

	// Step 1: create staging buffer.
	Buffer staging;
	BufferConfig stagingConfig;
	stagingConfig.size = static_cast<VkDeviceSize>(size);
	stagingConfig.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingConfig.memoryProperties =
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	RenderError err = staging.Initialize(device, stagingConfig);
	if (err != RenderError::None)
	{
		LOG_ERROR(logger, "Buffer::UploadViaStaging — staging buffer Initialize failed");
		return err;
	}

	// Step 2: host copy into staging buffer.
	err = staging.Upload(data, static_cast<VkDeviceSize>(size));
	if (err != RenderError::None)
	{
		LOG_ERROR(logger, "Buffer::UploadViaStaging — staging buffer Upload failed");
		return err;
	}

	// Step 3: allocate a one-shot command buffer.
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = transfer_pool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
	VkResult result = vkAllocateCommandBuffers(device.GetHandle(), &allocInfo, &cmdBuffer);
	if (result != VK_SUCCESS)
	{
		LOG_ERROR(logger,
			"Buffer::UploadViaStaging — vkAllocateCommandBuffers failed (VkResult {})",
			static_cast<int>(result));
		return RenderError::CommandPoolCreateFailed;
	}

	// Step 4: record the copy command.
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(cmdBuffer, &beginInfo);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = static_cast<VkDeviceSize>(size);

	vkCmdCopyBuffer(cmdBuffer, staging.GetHandle(), _buffer, 1, &copyRegion);

	vkEndCommandBuffer(cmdBuffer);

	// Step 5: submit and wait.
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	vkQueueSubmit(transfer_queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(transfer_queue);

	// Step 6: free the command buffer.
	vkFreeCommandBuffers(device.GetHandle(), transfer_pool, 1, &cmdBuffer);

	// staging Buffer is destroyed as it goes out of scope.
	return RenderError::None;
}

// ---------------------------------------------------------------------------
// Observers
// ---------------------------------------------------------------------------

VkBuffer Buffer::GetHandle() const noexcept
{
	return _buffer;
}

VkDeviceSize Buffer::GetSize() const noexcept
{
	return _size;
}

bool Buffer::IsInitialized() const noexcept
{
	return _buffer != VK_NULL_HANDLE;
}

VkDeviceAddress Buffer::GetDeviceAddress(const Device& device) const noexcept
{
	return device.GetBufferDeviceAddress(_buffer);
}

} // namespace virasa
