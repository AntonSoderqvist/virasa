#include "virasa/renderer/graph/ImageRegistry.h"

#include <cassert>

#include "quill/LogMacros.h"
#include "virasa/core/Logger.h"
#include "virasa/renderer/core/Device.h"

namespace virasa::renderer::graph
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static bool DescEqual(const GraphImageDesc& a, const GraphImageDesc& b) noexcept
{
	return a.width == b.width && a.height == b.height && a.format == b.format &&
		 a.usage == b.usage && a.aspect == b.aspect;
}

// ---------------------------------------------------------------------------
// Teardown helper (shared by destructor, move-assignment, and re-Initialize)
// ---------------------------------------------------------------------------

void ImageRegistry::Teardown()
{
	// Destroy Images in slot-index order (Image RAII destructor handles Vulkan cleanup).
	_slots.clear();
	_freeList.clear();
	_device = nullptr;
	_initialized = false;
}

// ---------------------------------------------------------------------------
// Special members
// ---------------------------------------------------------------------------

ImageRegistry::~ImageRegistry()
{
	Teardown();
}

ImageRegistry::ImageRegistry(ImageRegistry&& other) noexcept
    : _device(other._device), _initialized(other._initialized), _slots(std::move(other._slots)),
	_freeList(std::move(other._freeList))
{
	other._device = nullptr;
	other._initialized = false;
	// other._slots and other._freeList are already empty after the move.
}

ImageRegistry& ImageRegistry::operator=(ImageRegistry&& other) noexcept
{
	if (this != &other)
	{
		Teardown();

		_device = other._device;
		_initialized = other._initialized;
		_slots = std::move(other._slots);
		_freeList = std::move(other._freeList);

		other._device = nullptr;
		other._initialized = false;
	}
	return *this;
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

RenderError ImageRegistry::Initialize(const Device& device)
{
	if (_initialized)
	{
		// Re-initialization: tear down existing slots first.
		Teardown();
	}

	_device = &device;
	_initialized = true;
	return RenderError::None;
}

// ---------------------------------------------------------------------------
// Allocate
// ---------------------------------------------------------------------------

ImageHandle ImageRegistry::Allocate(const GraphImageDesc& desc)
{
	// Reject descriptors that would produce an invalid Vulkan image.
	if (desc.width == 0 || desc.height == 0 || desc.format == VK_FORMAT_UNDEFINED || desc.usage == 0)
		return ImageHandle{};

	// Reuse rule: scan free-slot list LIFO.
	for (int i = static_cast<int>(_freeList.size()) - 1; i >= 0; --i)
	{
		uint32_t idx = _freeList[static_cast<size_t>(i)];
		if (DescEqual(_slots[idx].desc, desc))
		{
			// Remove from free list.
			_freeList.erase(_freeList.begin() + i);
			_slots[idx].allocated = true;
			// ResourceUsage is intentionally preserved (cross-frame barrier state).
			return ImageHandle{idx};
		}
	}

	// Creation rule: build ImageConfig from desc.
	ImageConfig config;
	config.width = desc.width;
	config.height = desc.height;
	config.format = desc.format;
	config.usage = desc.usage;
	config.aspect = desc.aspect;
	config.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	config.flags = 0;

	Image image;
	RenderError err = image.Initialize(*_device, config);
	if (err != RenderError::None)
	{
		auto* logger = Logger::GetLogger("renderer");
		LOG_ERROR(logger, "ImageRegistry::Allocate — Image::Initialize failed");
		return ImageHandle{}; // id == 0xFFFFFFFFu (invalid sentinel)
	}

	uint32_t newIndex = static_cast<uint32_t>(_slots.size());
	Slot slot;
	slot.image = std::move(image);
	slot.desc = desc;
	slot.usage = ResourceUsage::Undefined;
	slot.allocated = true;
	_slots.push_back(std::move(slot));

	return ImageHandle{newIndex};
}

// ---------------------------------------------------------------------------
// Free
// ---------------------------------------------------------------------------

void ImageRegistry::Free(ImageHandle handle)
{
	assert(handle.IsValid());
	assert(handle.id < static_cast<uint32_t>(_slots.size()));
	assert(_slots[handle.id].allocated);

	_slots[handle.id].allocated = false;
	_freeList.push_back(handle.id);
	// Image, desc, and usage are deliberately preserved.
}

// ---------------------------------------------------------------------------
// Observers
// ---------------------------------------------------------------------------

const Image& ImageRegistry::Get(ImageHandle handle) const noexcept
{
	return _slots[handle.id].image;
}

const GraphImageDesc& ImageRegistry::GetDesc(ImageHandle handle) const noexcept
{
	return _slots[handle.id].desc;
}

ResourceUsage ImageRegistry::GetUsage(ImageHandle handle) const noexcept
{
	return _slots[handle.id].usage;
}

void ImageRegistry::SetUsage(ImageHandle handle, ResourceUsage usage)
{
	_slots[handle.id].usage = usage;
}

bool ImageRegistry::IsAllocated(ImageHandle handle) const noexcept
{
	if (!handle.IsValid())
		return false;
	if (handle.id >= static_cast<uint32_t>(_slots.size()))
		return false;
	return _slots[handle.id].allocated;
}

uint32_t ImageRegistry::GetSlotCount() const noexcept
{
	return static_cast<uint32_t>(_slots.size());
}

uint32_t ImageRegistry::GetAllocatedCount() const noexcept
{
	return static_cast<uint32_t>(_slots.size() - _freeList.size());
}

bool ImageRegistry::IsInitialized() const noexcept
{
	return _initialized;
}

} // namespace virasa::renderer::graph
