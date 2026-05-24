#include "virasa/renderer/graph/BufferRegistry.h"

#include <cassert>
#include <utility>

#include "quill/LogMacros.h"
#include "virasa/core/Logger.h"
#include "virasa/renderer/core/Device.h"

namespace virasa::renderer::graph
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static bool DescEqual(const GraphBufferDesc& a, const GraphBufferDesc& b) noexcept
{
	return a.size == b.size && a.usage == b.usage && a.hostVisible == b.hostVisible;
}

// ---------------------------------------------------------------------------
// Destructor / special members
// ---------------------------------------------------------------------------

BufferRegistry::~BufferRegistry()
{
	Teardown();
}

BufferRegistry::BufferRegistry(BufferRegistry&& other) noexcept
    : _device(other._device), _initialized(other._initialized), _slots(std::move(other._slots)),
	_freeList(std::move(other._freeList))
{
	other._device = nullptr;
	other._initialized = false;
}

BufferRegistry& BufferRegistry::operator=(BufferRegistry&& other) noexcept
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
// Private helpers
// ---------------------------------------------------------------------------

void BufferRegistry::Teardown()
{
	// Destroy slots in index order (Buffer RAII destructor handles Vulkan cleanup).
	_slots.clear();
	_freeList.clear();
	_device = nullptr;
	_initialized = false;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

RenderError BufferRegistry::Initialize(const Device& device)
{
	if (_initialized)
	{
		Teardown();
	}

	_device = &device;
	_initialized = true;
	return RenderError::None;
}

BufferHandle BufferRegistry::Allocate(const graph::GraphBufferDesc& desc)
{
	// Reuse rule: scan free-list LIFO.
	for (int i = static_cast<int>(_freeList.size()) - 1; i >= 0; --i)
	{
		uint32_t idx = _freeList[static_cast<size_t>(i)];
		if (DescEqual(_slots[idx].desc, desc))
		{
			_freeList.erase(_freeList.begin() + i);
			_slots[idx].allocated = true;
			return BufferHandle{idx};
		}
	}

	// Creation rule.
	Buffer buf;

	BufferConfig config;
	config.size = desc.size;
	config.usage = desc.usage;
	if (desc.hostVisible)
	{
		config.memoryProperties =
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	}
	else
	{
		config.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}

	RenderError err = buf.Initialize(*_device, config);
	if (err != RenderError::None)
	{
		auto* logger = Logger::GetLogger("renderer");
		LOG_ERROR(logger, "BufferRegistry::Allocate: Buffer::Initialize failed");
		return BufferHandle{}; // invalid sentinel (id == 0xFFFFFFFFu)
	}

	uint32_t newIdx = static_cast<uint32_t>(_slots.size());

	Slot slot;
	slot.buffer = std::move(buf);
	slot.desc = desc;
	slot.usage = ResourceUsage::Undefined;
	slot.allocated = true;
	_slots.push_back(std::move(slot));

	return BufferHandle{newIdx};
}

void BufferRegistry::Free(BufferHandle handle)
{
	assert(handle.IsValid());
	assert(handle.id < static_cast<uint32_t>(_slots.size()));
	assert(_slots[handle.id].allocated);

	_slots[handle.id].allocated = false;
	_freeList.push_back(handle.id);
}

const Buffer& BufferRegistry::Get(BufferHandle handle) const noexcept
{
	return _slots[handle.id].buffer;
}

const GraphBufferDesc& BufferRegistry::GetDesc(graph::BufferHandle handle) const noexcept
{
	return _slots[handle.id].desc;
}

ResourceUsage BufferRegistry::GetUsage(graph::BufferHandle handle) const noexcept
{
	return _slots[handle.id].usage;
}

void BufferRegistry::SetUsage(BufferHandle handle, graph::ResourceUsage usage)
{
	_slots[handle.id].usage = usage;
}

bool BufferRegistry::IsAllocated(BufferHandle handle) const noexcept
{
	if (!handle.IsValid())
	{
		return false;
	}
	if (handle.id >= static_cast<uint32_t>(_slots.size()))
	{
		return false;
	}
	return _slots[handle.id].allocated;
}

uint32_t BufferRegistry::GetSlotCount() const noexcept
{
	return static_cast<uint32_t>(_slots.size());
}

uint32_t BufferRegistry::GetAllocatedCount() const noexcept
{
	return static_cast<uint32_t>(_slots.size() - _freeList.size());
}

bool BufferRegistry::IsInitialized() const noexcept
{
	return _initialized;
}

} // namespace virasa::renderer::graph
