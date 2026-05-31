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
	: _buffers(std::move(other._buffers)),
	  _mappedPtrs(std::move(other._mappedPtrs)),
	  _bufferAddresses(std::move(other._bufferAddresses)),
	  _lightCounts(std::move(other._lightCounts)),
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
LightTable& LightTable::operator=(LightTable&& other) noexcept
{
	if (this != &other)
	{
		_buffers = std::move(other._buffers);
		_mappedPtrs = std::move(other._mappedPtrs);
		_bufferAddresses = std::move(other._bufferAddresses);
		_lightCounts = std::move(other._lightCounts);
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
RenderError LightTable::Initialize(
	const Device& device,
	uint32_t max_lights,
	uint32_t frames_in_flight)
{
	// Tear down any existing resources.
	_buffers.clear();
	_mappedPtrs.clear();
	_bufferAddresses.clear();
	_lightCounts.clear();
	_capacity = 0;
	_framesInFlight = 0;
	_currentFrame = 0;

	const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(max_lights) * sizeof(LightGPU);

	_buffers.resize(frames_in_flight);
	_mappedPtrs.resize(frames_in_flight, nullptr);
	_bufferAddresses.resize(frames_in_flight, 0);
	_lightCounts.resize(frames_in_flight, 0);

	for (uint32_t i = 0; i < frames_in_flight; ++i)
	{
		// Step 1: create light buffer i.
		BufferConfig config;
		config.size = bufferSize;
		config.usage =
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		config.memoryProperties =
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		RenderError err = _buffers[i].Initialize(device, config);
		if (err != RenderError::None)
		{
			// Tear down already-created entries.
			_buffers.clear();
			_mappedPtrs.clear();
			_bufferAddresses.clear();
			_lightCounts.clear();
			return err;
		}

		// Step 2: map light buffer i persistently.
		void* mapped = _buffers[i].Map();
		if (mapped == nullptr)
		{
			quill::Logger* logger = virasa::Logger::GetLogger("renderer");
			LOG_ERROR(logger,
				"LightTable::Initialize — failed to map light buffer for ring entry {}",
				i);
			_buffers.clear();
			_mappedPtrs.clear();
			_bufferAddresses.clear();
			_lightCounts.clear();
			return RenderError::MemoryMapFailed;
		}
		_mappedPtrs[i] = mapped;
	}

	// Step 3: cache observers.
	_capacity = max_lights;
	_framesInFlight = frames_in_flight;
	_currentFrame = 0;

	for (uint32_t i = 0; i < frames_in_flight; ++i)
	{
		_bufferAddresses[i] =
			_buffers[i].GetDeviceAddress(const_cast<Device&>(device));
		_lightCounts[i] = 0;
	}

	return RenderError::None;
}

// ---------------------------------------------------------------------------
// SetFrameIndex
// ---------------------------------------------------------------------------
void LightTable::SetFrameIndex(uint32_t frame_index)
{
	_currentFrame = frame_index;
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
		quill::Logger* logger = virasa::Logger::GetLogger("renderer");
		LOG_WARNING(logger,
			"LightTable::UploadFrame — {} light(s) dropped (capacity {})",
			n - _capacity,
			_capacity);
	}

	if (w > 0)
	{
		std::memcpy(
			_mappedPtrs[_currentFrame],
			lights.data(),
			static_cast<size_t>(w) * sizeof(LightGPU));
	}

	_lightCounts[_currentFrame] = w;
	return w;
}

// ---------------------------------------------------------------------------
// Observers
// ---------------------------------------------------------------------------
VkDeviceAddress LightTable::GetBufferAddress() const noexcept
{
	if (_bufferAddresses.empty())
	{
		return 0;
	}
	return _bufferAddresses[_currentFrame];
}

uint32_t LightTable::GetCapacity() const noexcept
{
	return _capacity;
}

uint32_t LightTable::GetLightCount() const noexcept
{
	if (_lightCounts.empty())
	{
		return 0;
	}
	return _lightCounts[_currentFrame];
}

bool LightTable::IsInitialized() const noexcept
{
	return !_bufferAddresses.empty() && _bufferAddresses[_currentFrame] != 0;
}

} // namespace virasa
