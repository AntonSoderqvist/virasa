#include "virasa/renderer/lighting/ShadowTable.h"

#include <quill/LogMacros.h>

#include <algorithm>
#include <cstring>

#include "virasa/core/Logger.h"
#include "virasa/renderer/core/Device.h"

namespace virasa
{

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

ShadowTable::~ShadowTable()
{
	// Buffer's own RAII destructor handles unmapping and freeing Vulkan resources.
	// We just reset our cached scalars; the Buffer destructor does the real work.
	_bufferAddress = 0;
	_capacity = 0;
	_shadowCount = 0;
	_mapped = nullptr;
}

// ---------------------------------------------------------------------------
// Move constructor / move assignment
// ---------------------------------------------------------------------------

ShadowTable::ShadowTable(ShadowTable&& other) noexcept
    : _buffer(std::move(other._buffer)), _bufferAddress(other._bufferAddress),
	_capacity(other._capacity), _shadowCount(other._shadowCount), _mapped(other._mapped)
{
	other._bufferAddress = 0;
	other._capacity = 0;
	other._shadowCount = 0;
	other._mapped = nullptr;
}

ShadowTable& ShadowTable::operator=(ShadowTable&& other) noexcept
{
	if (this != &other)
	{
		// Tear down existing resources via Buffer's move assignment (which
		// destroys the old buffer before taking ownership of the new one).
		_buffer = std::move(other._buffer);
		_bufferAddress = other._bufferAddress;
		_capacity = other._capacity;
		_shadowCount = other._shadowCount;
		_mapped = other._mapped;

		other._bufferAddress = 0;
		other._capacity = 0;
		other._shadowCount = 0;
		other._mapped = nullptr;
	}
	return *this;
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

RenderError ShadowTable::Initialize(const Device& device, uint32_t max_shadows)
{
	// If already initialized, tear down first (Buffer::Initialize handles this
	// internally, but we also need to reset our own cached state).
	if (_buffer.IsInitialized())
	{
		_buffer.Unmap();
		// Re-assign to a fresh Buffer to destroy the old one.
		_buffer = Buffer{};
		_bufferAddress = 0;
		_capacity = 0;
		_shadowCount = 0;
		_mapped = nullptr;
	}

	// Step 1: create shadow buffer.
	BufferConfig config;
	config.size = static_cast<VkDeviceSize>(max_shadows) * sizeof(ShadowGPU);
	config.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	config.memoryProperties =
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	RenderError err = _buffer.Initialize(device, config);
	if (err != RenderError::None)
	{
		return err;
	}

	// Step 2: map shadow buffer persistently.
	void* mapped = _buffer.Map();
	if (mapped == nullptr)
	{
		auto* logger = virasa::Logger::GetLogger("renderer");
		LOG_ERROR(logger, "ShadowTable::Initialize: failed to map shadow buffer");
		_buffer = Buffer{};
		return RenderError::MemoryMapFailed;
	}
	_mapped = mapped;

	// Step 3: cache observers.
	_capacity = max_shadows;
	_bufferAddress = _buffer.GetDeviceAddress(device);
	_shadowCount = 0;

	return RenderError::None;
}

// ---------------------------------------------------------------------------
// UploadFrame
// ---------------------------------------------------------------------------

uint32_t ShadowTable::UploadFrame(std::span<const ShadowGPU> shadows)
{
	const uint32_t n = static_cast<uint32_t>(shadows.size());
	const uint32_t w = std::min(n, _capacity);

	if (n > _capacity)
	{
		auto* logger = virasa::Logger::GetLogger("renderer");
		LOG_WARNING(logger,
			"ShadowTable::UploadFrame: {} shadow record(s) dropped (capacity {})",
			n - _capacity,
			_capacity);
	}

	if (w > 0)
	{
		std::memcpy(_mapped, shadows.data(), static_cast<size_t>(w) * sizeof(ShadowGPU));
	}

	_shadowCount = w;
	return w;
}

// ---------------------------------------------------------------------------
// Observers
// ---------------------------------------------------------------------------

VkDeviceAddress ShadowTable::GetBufferAddress() const noexcept
{
	return _bufferAddress;
}

uint32_t ShadowTable::GetCapacity() const noexcept
{
	return _capacity;
}

uint32_t ShadowTable::GetShadowCount() const noexcept
{
	return _shadowCount;
}

bool ShadowTable::IsInitialized() const noexcept
{
	return _bufferAddress != 0;
}

} // namespace virasa
