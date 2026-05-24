#include "virasa/renderer/resources/MeshRegistry.h"

#include <cassert>
#include <utility>

#include "quill/LogMacros.h"
#include "virasa/core/Logger.h"

namespace virasa::renderer
{

static constexpr uint32_t kSentinelId = 0xFFFFFFFFu;
static constexpr uint32_t kMaxSlotCount = 0xFFFFFFFEu;

// ---------------------------------------------------------------------------
// Move constructor
// ---------------------------------------------------------------------------
MeshRegistry::MeshRegistry(MeshRegistry&& other) noexcept
    : _slots(std::move(other._slots)), _freeList(std::move(other._freeList))
{
}

// ---------------------------------------------------------------------------
// Move assignment
// ---------------------------------------------------------------------------
MeshRegistry& MeshRegistry::operator=(MeshRegistry&& other) noexcept
{
	if (this != &other)
	{
		// Tear down existing slots in slot-index order (destroys each Mesh).
		_slots.clear();
		_freeList.clear();

		_slots = std::move(other._slots);
		_freeList = std::move(other._freeList);
	}
	return *this;
}

// ---------------------------------------------------------------------------
// Allocate
// ---------------------------------------------------------------------------
uint32_t MeshRegistry::Allocate(Mesh&& mesh)
{
	// Defensive sentinel: slot count at maximum and no free slots available.
	if (_freeList.empty() && _slots.size() >= static_cast<size_t>(kMaxSlotCount))
	{
		auto* logger = Logger::GetLogger("renderer");
		LOG_ERROR(logger,
			"MeshRegistry::Allocate: slot count has reached the maximum (0xFFFFFFFE); "
			"cannot allocate a new slot.");
		return kSentinelId;
	}

	uint32_t id;

	if (!_freeList.empty())
	{
		// Reuse the last freed slot (stack discipline).
		id = _freeList.back();
		_freeList.pop_back();
		_slots[id] = std::move(mesh);
	}
	else
	{
		// Grow the slot collection.
		id = static_cast<uint32_t>(_slots.size());
		_slots.emplace_back(std::move(mesh));
	}

	return id;
}

// ---------------------------------------------------------------------------
// Free
// ---------------------------------------------------------------------------
void MeshRegistry::Free(uint32_t id)
{
	assert(id < _slots.size() && "MeshRegistry::Free: id out of range");
	assert(_slots[id].has_value() && "MeshRegistry::Free: slot is not currently allocated");

	// Destroy the Mesh (runs its RAII destructor, releasing Vulkan resources).
	_slots[id].reset();

	// Return the slot index to the free list.
	_freeList.push_back(id);
}

// ---------------------------------------------------------------------------
// Get
// ---------------------------------------------------------------------------
const Mesh& MeshRegistry::Get(uint32_t id) const noexcept
{
	// Precondition: caller must only call Get with a currently-allocated id.
	return *_slots[id];
}

// ---------------------------------------------------------------------------
// IsAllocated
// ---------------------------------------------------------------------------
bool MeshRegistry::IsAllocated(uint32_t id) const noexcept
{
	if (id >= static_cast<uint32_t>(_slots.size()))
	{
		return false;
	}
	return _slots[id].has_value();
}

// ---------------------------------------------------------------------------
// GetSlotCount
// ---------------------------------------------------------------------------
uint32_t MeshRegistry::GetSlotCount() const noexcept
{
	return static_cast<uint32_t>(_slots.size());
}

// ---------------------------------------------------------------------------
// GetAllocatedCount
// ---------------------------------------------------------------------------
uint32_t MeshRegistry::GetAllocatedCount() const noexcept
{
	return static_cast<uint32_t>(_slots.size()) - static_cast<uint32_t>(_freeList.size());
}

} // namespace virasa::renderer
