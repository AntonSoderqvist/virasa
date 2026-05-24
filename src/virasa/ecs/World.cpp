#include "virasa/ecs/World.h"

#include <cassert>
#include <algorithm>

namespace virasa::ecs
{

static constexpr uint32_t kSparseNone = 0xFFFFFFFFu;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void World::EnsureSparseCapacity(std::vector<uint32_t>& sparse, uint32_t index)
{
	if (index >= static_cast<uint32_t>(sparse.size()))
	{
		sparse.resize(static_cast<size_t>(index) + 1u, kSparseNone);
	}
}

void World::RemoveTransformComponentInternal(virasa::ecs::Entity entity)
{
	if (!HasTransformComponent(entity))
		return;

	const uint32_t denseIdx = _transformSparse[entity.index];
	const uint32_t lastIdx  = static_cast<uint32_t>(_transformValues.size()) - 1u;

	if (denseIdx != lastIdx)
	{
		// Swap with last
		_transformValues[denseIdx]   = std::move(_transformValues[lastIdx]);
		_transformEntities[denseIdx] = _transformEntities[lastIdx];
		_transformSparse[_transformEntities[denseIdx].index] = denseIdx;
	}

	_transformValues.pop_back();
	_transformEntities.pop_back();
	_transformSparse[entity.index] = kSparseNone;
}

void World::RemoveMeshComponentInternal(virasa::ecs::Entity entity)
{
	if (!HasMeshComponent(entity))
		return;

	const uint32_t denseIdx = _meshSparse[entity.index];
	const uint32_t lastIdx  = static_cast<uint32_t>(_meshValues.size()) - 1u;

	if (denseIdx != lastIdx)
	{
		_meshValues[denseIdx]   = _meshValues[lastIdx];
		_meshEntities[denseIdx] = _meshEntities[lastIdx];
		_meshSparse[_meshEntities[denseIdx].index] = denseIdx;
	}

	_meshValues.pop_back();
	_meshEntities.pop_back();
	_meshSparse[entity.index] = kSparseNone;
}

void World::RemoveVisualComponentInternal(virasa::ecs::Entity entity)
{
	if (!HasVisualComponent(entity))
		return;

	const uint32_t denseIdx = _visualSparse[entity.index];
	const uint32_t lastIdx  = static_cast<uint32_t>(_visualValues.size()) - 1u;

	if (denseIdx != lastIdx)
	{
		_visualValues[denseIdx]   = _visualValues[lastIdx];
		_visualEntities[denseIdx] = _visualEntities[lastIdx];
		_visualSparse[_visualEntities[denseIdx].index] = denseIdx;
	}

	_visualValues.pop_back();
	_visualEntities.pop_back();
	_visualSparse[entity.index] = kSparseNone;
}

void World::DestroyEntityInternal(virasa::ecs::Entity entity, bool detachFromParent)
{
	// Recursively destroy children first (depth-first)
	// Copy children vector because we'll be mutating it during recursion
	std::vector<virasa::ecs::Entity> childrenCopy = _children[entity.index];
	for (virasa::ecs::Entity child : childrenCopy)
	{
		// Children don't need to detach from this entity (it's being destroyed)
		DestroyEntityInternal(child, false);
	}

	// (a) Remove components
	RemoveTransformComponentInternal(entity);
	RemoveMeshComponentInternal(entity);
	RemoveVisualComponentInternal(entity);

	// (b) Hierarchy detachment
	if (detachFromParent)
	{
		virasa::ecs::Entity parentEntity = _parents[entity.index];
		if (parentEntity != virasa::ecs::Entity::Invalid())
		{
			auto& parentChildren = _children[parentEntity.index];
			parentChildren.erase(
				std::remove(parentChildren.begin(), parentChildren.end(), entity),
				parentChildren.end()
			);
		}
	}

	// Clear this entity's children vector
	_children[entity.index].clear();
	_parents[entity.index] = virasa::ecs::Entity::Invalid();

	// (c) Generation bump
	_generations[entity.index]++;
	_alive[entity.index] = false;

	// (d) Free-list push
	_freeList.push_back(entity.index);

	--_entityCount;
}

// ---------------------------------------------------------------------------
// Move operations
// ---------------------------------------------------------------------------

World::World(World&& other) noexcept
	: _generations(std::move(other._generations))
	, _alive(std::move(other._alive))
	, _parents(std::move(other._parents))
	, _children(std::move(other._children))
	, _freeList(std::move(other._freeList))
	, _entityCount(other._entityCount)
	, _transformValues(std::move(other._transformValues))
	, _transformEntities(std::move(other._transformEntities))
	, _transformSparse(std::move(other._transformSparse))
	, _meshValues(std::move(other._meshValues))
	, _meshEntities(std::move(other._meshEntities))
	, _meshSparse(std::move(other._meshSparse))
	, _visualValues(std::move(other._visualValues))
	, _visualEntities(std::move(other._visualEntities))
	, _visualSparse(std::move(other._visualSparse))
{
	other._entityCount = 0u;
}

World& World::operator=(World&& other) noexcept
{
	if (this != &other)
	{
		_generations       = std::move(other._generations);
		_alive             = std::move(other._alive);
		_freeList          = std::move(other._freeList);
		_parents           = std::move(other._parents);
		_children          = std::move(other._children);
		_entityCount       = other._entityCount;
		_transformValues   = std::move(other._transformValues);
		_transformEntities = std::move(other._transformEntities);
		_transformSparse   = std::move(other._transformSparse);
		_meshValues        = std::move(other._meshValues);
		_meshEntities      = std::move(other._meshEntities);
		_meshSparse        = std::move(other._meshSparse);
		_visualValues      = std::move(other._visualValues);
		_visualEntities    = std::move(other._visualEntities);
		_visualSparse      = std::move(other._visualSparse);
		other._entityCount = 0u;
	}
	return *this;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

virasa::ecs::Entity World::CreateEntity()
{
	uint32_t slotIndex = 0u;
	uint32_t generation = 0u;

	if (!_freeList.empty())
	{
		slotIndex = _freeList.back();
		_freeList.pop_back();
		generation = _generations[slotIndex];
		_alive[slotIndex] = true;
		_parents[slotIndex] = virasa::ecs::Entity::Invalid();
		_children[slotIndex].clear();
	}
	else
	{
		slotIndex = static_cast<uint32_t>(_generations.size());
		generation = 0u;
		_generations.push_back(0u);
		_alive.push_back(true);
		_parents.push_back(virasa::ecs::Entity::Invalid());
		_children.emplace_back();
	}

	++_entityCount;

	virasa::ecs::Entity entity;
	entity.index      = slotIndex;
	entity.generation = generation;
	return entity;
}

void World::DestroyEntity(virasa::ecs::Entity entity)
{
	DestroyEntityInternal(entity, true);
}

bool World::IsValid(virasa::ecs::Entity entity) const noexcept
{
	if (entity.index >= static_cast<uint32_t>(_generations.size()))
		return false;
	if (!_alive[entity.index])
		return false;
	return _generations[entity.index] == entity.generation;
}

void World::Reserve(uint32_t entityCapacity)
{
	_generations.reserve(entityCapacity);
	_alive.reserve(entityCapacity);
	_parents.reserve(entityCapacity);
	_children.reserve(entityCapacity);
}

uint32_t World::GetEntityCount() const noexcept
{
	return _entityCount;
}

virasa::ecs::EcsError World::SetParent(virasa::ecs::Entity child, virasa::ecs::Entity parent)
{
	// (1) Validate child
	if (child == virasa::ecs::Entity::Invalid() || !IsValid(child))
		return virasa::ecs::EcsError::InvalidEntity;

	// (2) Validate parent (if not Invalid)
	if (parent != virasa::ecs::Entity::Invalid() && !IsValid(parent))
		return virasa::ecs::EcsError::InvalidEntity;

	// (3) Self-parent check
	if (parent == child)
		return virasa::ecs::EcsError::SelfParent;

	// (4) Cycle detection: walk up from parent, check if child appears
	if (parent != virasa::ecs::Entity::Invalid())
	{
		virasa::ecs::Entity ancestor = parent;
		while (ancestor != virasa::ecs::Entity::Invalid())
		{
			if (ancestor == child)
				return virasa::ecs::EcsError::CycleDetected;
			ancestor = _parents[ancestor.index];
		}
	}

	// (5) Perform reparent
	// Detach from current parent
	virasa::ecs::Entity currentParent = _parents[child.index];
	if (currentParent != virasa::ecs::Entity::Invalid())
	{
		auto& siblings = _children[currentParent.index];
		siblings.erase(
			std::remove(siblings.begin(), siblings.end(), child),
			siblings.end()
		);
	}

	// Set new parent
	_parents[child.index] = parent;

	// Attach to new parent
	if (parent != virasa::ecs::Entity::Invalid())
	{
		_children[parent.index].push_back(child);
	}

	return virasa::ecs::EcsError::None;
}

virasa::ecs::Entity World::GetParent(virasa::ecs::Entity entity) const noexcept
{
	return _parents[entity.index];
}

const std::vector<virasa::ecs::Entity>& World::GetChildren(virasa::ecs::Entity entity) const
{
	return _children[entity.index];
}

// ---------------------------------------------------------------------------
// Transform component
// ---------------------------------------------------------------------------

void World::AddTransformComponent(virasa::ecs::Entity entity, const virasa::math::Transform& transform)
{
	EnsureSparseCapacity(_transformSparse, entity.index);

	const uint32_t denseIdx = static_cast<uint32_t>(_transformValues.size());
	_transformValues.push_back(transform);
	_transformEntities.push_back(entity);
	_transformSparse[entity.index] = denseIdx;
}

void World::RemoveTransformComponent(virasa::ecs::Entity entity)
{
	RemoveTransformComponentInternal(entity);
}

bool World::HasTransformComponent(virasa::ecs::Entity entity) const noexcept
{
	if (!IsValid(entity))
		return false;
	if (entity.index >= static_cast<uint32_t>(_transformSparse.size()))
		return false;
	const uint32_t denseIdx = _transformSparse[entity.index];
	if (denseIdx == kSparseNone)
		return false;
	return _transformEntities[denseIdx] == entity;
}

const virasa::math::Transform& World::GetTransformComponent(virasa::ecs::Entity entity) const
{
	return _transformValues[_transformSparse[entity.index]];
}

virasa::math::Transform& World::GetTransformComponent(virasa::ecs::Entity entity)
{
	return _transformValues[_transformSparse[entity.index]];
}

const std::vector<virasa::ecs::Entity>& World::GetTransformComponentEntities() const noexcept
{
	return _transformEntities;
}

// ---------------------------------------------------------------------------
// MeshComponent
// ---------------------------------------------------------------------------

void World::AddMeshComponent(virasa::ecs::Entity entity, virasa::ecs::MeshComponent component)
{
	EnsureSparseCapacity(_meshSparse, entity.index);

	const uint32_t denseIdx = static_cast<uint32_t>(_meshValues.size());
	_meshValues.push_back(component);
	_meshEntities.push_back(entity);
	_meshSparse[entity.index] = denseIdx;
}

void World::RemoveMeshComponent(virasa::ecs::Entity entity)
{
	RemoveMeshComponentInternal(entity);
}

bool World::HasMeshComponent(virasa::ecs::Entity entity) const noexcept
{
	if (!IsValid(entity))
		return false;
	if (entity.index >= static_cast<uint32_t>(_meshSparse.size()))
		return false;
	const uint32_t denseIdx = _meshSparse[entity.index];
	if (denseIdx == kSparseNone)
		return false;
	return _meshEntities[denseIdx] == entity;
}

const virasa::ecs::MeshComponent& World::GetMeshComponent(virasa::ecs::Entity entity) const
{
	return _meshValues[_meshSparse[entity.index]];
}

virasa::ecs::MeshComponent& World::GetMeshComponent(virasa::ecs::Entity entity)
{
	return _meshValues[_meshSparse[entity.index]];
}

const std::vector<virasa::ecs::Entity>& World::GetMeshComponentEntities() const noexcept
{
	return _meshEntities;
}

// ---------------------------------------------------------------------------
// VisualComponent
// ---------------------------------------------------------------------------

void World::AddVisualComponent(virasa::ecs::Entity entity, virasa::ecs::VisualComponent component)
{
	EnsureSparseCapacity(_visualSparse, entity.index);

	const uint32_t denseIdx = static_cast<uint32_t>(_visualValues.size());
	_visualValues.push_back(component);
	_visualEntities.push_back(entity);
	_visualSparse[entity.index] = denseIdx;
}

void World::RemoveVisualComponent(virasa::ecs::Entity entity)
{
	RemoveVisualComponentInternal(entity);
}

bool World::HasVisualComponent(virasa::ecs::Entity entity) const noexcept
{
	if (!IsValid(entity))
		return false;
	if (entity.index >= static_cast<uint32_t>(_visualSparse.size()))
		return false;
	const uint32_t denseIdx = _visualSparse[entity.index];
	if (denseIdx == kSparseNone)
		return false;
	return _visualEntities[denseIdx] == entity;
}

const virasa::ecs::VisualComponent& World::GetVisualComponent(virasa::ecs::Entity entity) const
{
	return _visualValues[_visualSparse[entity.index]];
}

virasa::ecs::VisualComponent& World::GetVisualComponent(virasa::ecs::Entity entity)
{
	return _visualValues[_visualSparse[entity.index]];
}

const std::vector<virasa::ecs::Entity>& World::GetVisualComponentEntities() const noexcept
{
	return _visualEntities;
}

} // namespace virasa::ecs
