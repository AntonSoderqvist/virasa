#include "virasa/renderer/lighting/LightTable.h"

#include <quill/LogMacros.h>

#include <algorithm>
#include <cstring>

#include "virasa/core/Logger.h"

namespace virasa
{

// ---------------------------------------------------------------------------
// Move constructor
// ---------------------------------------------------------------------------
LightTable::LightTable(LightTable&& other) noexcept
    : _buffer(std::move(other._buffer)), _mapped(other._mapped), _capacity(other._capacity),
	_bufferAddress(other._bufferAddress), _lightCount(other._lightCount)
{
	other._mapped = nullptr;
	other._capacity = 0;
	other._bufferAddress = 0;
	other._lightCount = 0;
}

// ---------------------------------------------------------------------------
// Move assignment
// ---------------------------------------------------------------------------
LightTable& LightTable::operator=(LightTable&& other) noexcept
{
	if (this != &other)
	{
		// Tear down existing resources (Buffer RAII handles Vulkan cleanup).
		_buffer = std::move(other._buffer);
		_mapped = other._mapped;
		_capacity = other._capacity;
		_bufferAddress = other._bufferAddress;
		_lightCount = other._lightCount;

		other._mapped = nullptr;
		other._capacity = 0;
		other._bufferAddress = 0;
		other._lightCount = 0;
	}
	return *this;
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------
RenderError LightTable::Initialize(const Device& device, uint32_t max_lights)
{
	// Tear down any existing resources by replacing _buffer with a fresh one.
	// Buffer's move-assignment will destroy the old buffer via its RAII dtor.
	_buffer = Buffer{};
	_mapped = nullptr;
	_capacity = 0;
	_bufferAddress = 0;
	_lightCount = 0;

	// Step 1: create the light buffer.
	BufferConfig config;
	config.size = static_cast<VkDeviceSize>(max_lights) * sizeof(LightGPU);
	config.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	config.memoryProperties =
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	RenderError err = _buffer.Initialize(device, config);
	if (err != RenderError::None)
	{
		return err;
	}

	// Step 2: map the buffer persistently.
	void* mapped = _buffer.Map();
	if (mapped == nullptr)
	{
		quill::Logger* logger = Logger::GetLogger("renderer");
		LOG_ERROR(logger, "LightTable::Initialize — failed to map light buffer");
		_buffer = Buffer{};
		return RenderError::MemoryMapFailed;
	}
	_mapped = mapped;

	// Step 3: cache observers.
	_capacity = max_lights;
	// GetDeviceAddress requires a non-const Device reference in the Buffer API,
	// but the contract takes a const Device&. We cast away const here because
	// GetDeviceAddress is semantically a pure observer (vkGetBufferDeviceAddress
	// cannot fail and does not mutate the device).
	_bufferAddress = _buffer.GetDeviceAddress(const_cast<Device&>(device));
	_lightCount = 0;

	return RenderError::None;
}

// ---------------------------------------------------------------------------
// UploadFrame
// ---------------------------------------------------------------------------
uint32_t LightTable::UploadFrame(std::span<const LightGPU> lights)
{
	const uint32_t n = static_cast<uint32_t>(lights.size());
	const uint32_t w = std::min(n, _capacity);

	if (n > _capacity)
	{
		quill::Logger* logger = Logger::GetLogger("renderer");
		LOG_WARNING(logger,
			"LightTable::UploadFrame — {} light(s) dropped (capacity {})",
			n - _capacity,
			_capacity);
	}

	if (w > 0)
	{
		std::memcpy(_mapped, lights.data(), static_cast<size_t>(w) * sizeof(LightGPU));
	}

	_lightCount = w;
	return w;
}

// ---------------------------------------------------------------------------
// Observers
// ---------------------------------------------------------------------------
VkDeviceAddress LightTable::GetBufferAddress() const noexcept
{
	return _bufferAddress;
}

uint32_t LightTable::GetCapacity() const noexcept
{
	return _capacity;
}

uint32_t LightTable::GetLightCount() const noexcept
{
	return _lightCount;
}

bool LightTable::IsInitialized() const noexcept
{
	return _bufferAddress != 0;
}

} // namespace virasa
