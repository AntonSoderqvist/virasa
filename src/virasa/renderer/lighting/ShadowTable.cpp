#include "virasa/renderer/lighting/ShadowTable.h"

#include <quill/LogMacros.h>

#include <algorithm>
#include <cstring>

#include "virasa/core/Logger.h"

namespace virasa
{

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
ShadowTable::~ShadowTable()
{
	// Each ring Buffer's own RAII destructor unmaps and frees its Vulkan
	// resources when _buffers is destroyed; nothing else to do here.
}

// ---------------------------------------------------------------------------
// Move constructor
// ---------------------------------------------------------------------------
ShadowTable::ShadowTable(ShadowTable&& other) noexcept
	: _buffers(std::move(other._buffers)),
	  _mappedPtrs(std::move(other._mappedPtrs)),
	  _bufferAddresses(std::move(other._bufferAddresses)),
	  _shadowCounts(std::move(other._shadowCounts)),
	  _capacity(other._capacity),
	  _framesInFlight(other._framesInFlight),
	  _currentFrame(other._currentFrame)
{
	other._capacity = 0;
	other._framesInFlight = 0;
	other._currentFrame = 0;
}

// ---------------------------------------------------------------------------
// Move assignment
// ---------------------------------------------------------------------------
ShadowTable& ShadowTable::operator=(ShadowTable&& other) noexcept
{
	if (this != &other)
	{
		_buffers = std::move(other._buffers);
		_mappedPtrs = std::move(other._mappedPtrs);
		_bufferAddresses = std::move(other._bufferAddresses);
		_shadowCounts = std::move(other._shadowCounts);
		_capacity = other._capacity;
		_framesInFlight = other._framesInFlight;
		_currentFrame = other._currentFrame;

		other._capacity = 0;
		other._framesInFlight = 0;
		other._currentFrame = 0;
	}
	return *this;
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------
RenderError ShadowTable::Initialize(
	const Device& device,
	uint32_t max_shadows,
	uint32_t frames_in_flight)
{
	// Tear down any existing resources.
	_buffers.clear();
	_mappedPtrs.clear();
	_bufferAddresses.clear();
	_shadowCounts.clear();
	_capacity = 0;
	_framesInFlight = 0;
	_currentFrame = 0;

	const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(max_shadows) * sizeof(ShadowGPU);

	_buffers.resize(frames_in_flight);
	_mappedPtrs.resize(frames_in_flight, nullptr);
	_bufferAddresses.resize(frames_in_flight, 0);
	_shadowCounts.resize(frames_in_flight, 0);

	for (uint32_t i = 0; i < frames_in_flight; ++i)
	{
		// Step 1: create shadow buffer i.
		BufferConfig config;
		config.size = bufferSize;
		config.usage =
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		config.memoryProperties =
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		RenderError err = _buffers[i].Initialize(device, config);
		if (err != RenderError::None)
		{
			_buffers.clear();
			_mappedPtrs.clear();
			_bufferAddresses.clear();
			_shadowCounts.clear();
			return err;
		}

		// Step 2: map shadow buffer i persistently.
		void* mapped = _buffers[i].Map();
		if (mapped == nullptr)
		{
			quill::Logger* logger = virasa::Logger::GetLogger("renderer");
			LOG_ERROR(logger,
				"ShadowTable::Initialize — failed to map shadow buffer for ring entry {}",
				i);
			_buffers.clear();
			_mappedPtrs.clear();
			_bufferAddresses.clear();
			_shadowCounts.clear();
			return RenderError::MemoryMapFailed;
		}
		_mappedPtrs[i] = mapped;
	}

	// Step 3: cache observers.
	_capacity = max_shadows;
	_framesInFlight = frames_in_flight;
	_currentFrame = 0;

	for (uint32_t i = 0; i < frames_in_flight; ++i)
	{
		_bufferAddresses[i] =
			_buffers[i].GetDeviceAddress(const_cast<Device&>(device));
		_shadowCounts[i] = 0;
	}

	return RenderError::None;
}

// ---------------------------------------------------------------------------
// SetFrameIndex
// ---------------------------------------------------------------------------
void ShadowTable::SetFrameIndex(uint32_t frame_index)
{
	_currentFrame = frame_index;
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
		quill::Logger* logger = virasa::Logger::GetLogger("renderer");
		LOG_WARNING(logger,
			"ShadowTable::UploadFrame — {} shadow record(s) dropped (capacity {})",
			n - _capacity,
			_capacity);
	}

	if (w > 0)
	{
		std::memcpy(
			_mappedPtrs[_currentFrame],
			shadows.data(),
			static_cast<size_t>(w) * sizeof(ShadowGPU));
	}

	_shadowCounts[_currentFrame] = w;
	return w;
}

// ---------------------------------------------------------------------------
// Observers
// ---------------------------------------------------------------------------
VkDeviceAddress ShadowTable::GetBufferAddress() const noexcept
{
	if (_bufferAddresses.empty())
	{
		return 0;
	}
	return _bufferAddresses[_currentFrame];
}

uint32_t ShadowTable::GetCapacity() const noexcept
{
	return _capacity;
}

uint32_t ShadowTable::GetShadowCount() const noexcept
{
	if (_shadowCounts.empty())
	{
		return 0;
	}
	return _shadowCounts[_currentFrame];
}

bool ShadowTable::IsInitialized() const noexcept
{
	return !_bufferAddresses.empty() && _bufferAddresses[_currentFrame] != 0;
}

} // namespace virasa
