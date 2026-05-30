#include "virasa/ecs/ComponentSystem.h"

#include <cstring>
#include <algorithm>
#include <cassert>

namespace virasa::ecs
{

// ---------------------------------------------------------------------------
// SparseComponentSystem
// ---------------------------------------------------------------------------

SparseComponentSystem::SparseComponentSystem(virasa::ecs::ComponentId id, const char* name, size_t elementSize)
	: _id(id)
	, _name(name)
	, _elementSize(elementSize)
{
}

const char* SparseComponentSystem::Name() const noexcept
{
	return _name;
}

virasa::ecs::ComponentId SparseComponentSystem::Id() const noexcept
{
	return _id;
}

void SparseComponentSystem::SetId(virasa::ecs::ComponentId id) noexcept
{
	_id = id;
}

bool SparseComponentSystem::Has(virasa::ecs::Entity entity) const noexcept
{
	if (entity.index >= _sparse.size())
	{
		return false;
	}
	uint32_t denseIdx = _sparse[entity.index];
	if (denseIdx == kSparseSentinel)
	{
		return false;
	}
	return _denseEntities[denseIdx] == entity;
}

void* SparseComponentSystem::AddRaw(virasa::ecs::Entity entity, const void* value)
{
	assert(!Has(entity) && "AddRaw called for entity that already owns a component");

	// Grow sparse vector if needed
	if (entity.index >= _sparse.size())
	{
		_sparse.resize(static_cast<size_t>(entity.index) + 1u, kSparseSentinel);
	}

	uint32_t denseIdx = static_cast<uint32_t>(_denseEntities.size());

	// Append bytes
	size_t offset = _denseData.size();
	_denseData.resize(offset + _elementSize);
	std::memcpy(_denseData.data() + offset, value, _elementSize);

	// Append entity
	_denseEntities.push_back(entity);

	// Update sparse
	_sparse[entity.index] = denseIdx;

	// Mark dirty
	MarkDirty(entity);

	// Hook
	OnAdd(denseIdx);

	return _denseData.data() + offset;
}

void SparseComponentSystem::Remove(virasa::ecs::Entity entity)
{
	assert(Has(entity) && "Remove called for entity that does not own a component");

	uint32_t removedDenseIdx = _sparse[entity.index];
	uint32_t lastDenseIdx = static_cast<uint32_t>(_denseEntities.size()) - 1u;

	if (removedDenseIdx != lastDenseIdx)
	{
		// Swap last element into the removed slot
		virasa::ecs::Entity lastEntity = _denseEntities[lastDenseIdx];

		// Copy bytes
		std::memcpy(
			_denseData.data() + static_cast<size_t>(removedDenseIdx) * _elementSize,
			_denseData.data() + static_cast<size_t>(lastDenseIdx) * _elementSize,
			_elementSize
		);

		// Swap entity
		_denseEntities[removedDenseIdx] = lastEntity;

		// Update sparse for swapped entity
		_sparse[lastEntity.index] = removedDenseIdx;

		// Notify subclass
		OnRemoveSwap(removedDenseIdx, lastDenseIdx);
	}

	// Shrink dense collections
	_denseData.resize(_denseData.size() - _elementSize);
	_denseEntities.pop_back();

	// Reset sparse for removed entity
	_sparse[entity.index] = kSparseSentinel;

	// Remove from dirty set
	auto it = std::find_if(_dirty.begin(), _dirty.end(),
		[&entity](const virasa::ecs::Entity& e) { return e == entity; });
	if (it != _dirty.end())
	{
		_dirty.erase(it);
	}
}

size_t SparseComponentSystem::Size() const noexcept
{
	return _denseEntities.size();
}

const std::vector<virasa::ecs::Entity>& SparseComponentSystem::Entities() const noexcept
{
	return _denseEntities;
}

const void* SparseComponentSystem::GetRaw(virasa::ecs::Entity entity) const
{
	assert(Has(entity) && "GetRaw called for entity that does not own a component");
	uint32_t denseIdx = _sparse[entity.index];
	return _denseData.data() + static_cast<size_t>(denseIdx) * _elementSize;
}

void SparseComponentSystem::SetRaw(virasa::ecs::Entity entity, const void* value)
{
	assert(Has(entity) && "SetRaw called for entity that does not own a component");
	uint32_t denseIdx = _sparse[entity.index];
	std::memcpy(
		_denseData.data() + static_cast<size_t>(denseIdx) * _elementSize,
		value,
		_elementSize
	);
	MarkDirty(entity);
	OnSet(entity);
}

void SparseComponentSystem::ForEachRaw(
	const std::function<void(virasa::ecs::Entity, const void*)>& fn) const
{
	for (size_t i = 0; i < _denseEntities.size(); ++i)
	{
		fn(_denseEntities[i], _denseData.data() + i * _elementSize);
	}
}

const std::vector<virasa::ecs::Entity>& SparseComponentSystem::Dirty() const noexcept
{
	return _dirty;
}

void SparseComponentSystem::ClearDirty(virasa::ecs::Entity entity)
{
	auto it = std::find_if(_dirty.begin(), _dirty.end(),
		[&entity](const virasa::ecs::Entity& e) { return e == entity; });
	if (it != _dirty.end())
	{
		_dirty.erase(it);
	}
}

void SparseComponentSystem::ClearAllDirty()
{
	_dirty.clear();
}

void SparseComponentSystem::Update()
{
	// No-op for the base SparseComponentSystem.
}

void SparseComponentSystem::MarkDirty(virasa::ecs::Entity entity)
{
	for (const auto& e : _dirty)
	{
		if (e == entity)
		{
			return;
		}
	}
	_dirty.push_back(entity);
}

void SparseComponentSystem::OnAdd(uint32_t /*denseIndex*/)
{
	// Default: no-op.
}

void SparseComponentSystem::OnRemoveSwap(uint32_t /*removedDenseIndex*/, uint32_t /*lastDenseIndex*/)
{
	// Default: no-op.
}

void SparseComponentSystem::OnSet(virasa::ecs::Entity /*entity*/)
{
	// Default: no-op.
}

} // namespace virasa::ecs
