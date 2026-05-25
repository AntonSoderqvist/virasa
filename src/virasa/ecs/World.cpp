#include "virasa/ecs/World.h"

#include <algorithm>
#include <cassert>
#include <string>

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

void World::RemoveFromRoots(virasa::ecs::Entity entity) noexcept
{
	auto it = std::find(_roots.begin(), _roots.end(), entity);
	if (it != _roots.end())
	{
		*it = _roots.back();
		_roots.pop_back();
	}
}

void World::RemoveFromChildren(
	std::vector<virasa::ecs::Entity>& children, virasa::ecs::Entity entity) noexcept
{
	auto it = std::find(children.begin(), children.end(), entity);
	if (it != children.end())
	{
		*it = children.back();
		children.pop_back();
	}
}

std::string World::MakeUniqueEntityName(std::string_view name) const
{
	std::string baseName = name.empty() ? std::string("Entity") : std::string(name);

	auto nameExists = [this](std::string_view candidate) -> bool
	{
		for (size_t i = 0; i < _nameValues.size(); ++i)
		{
			if (_nameValues[i].name == candidate)
			{
				return true;
			}
		}
		return false;
	};

	if (!nameExists(baseName))
	{
		return baseName;
	}

	for (uint32_t suffix = 1u;; ++suffix)
	{
		std::string candidate = baseName;
		candidate.push_back('.');
		if (suffix <= 999u)
		{
			if (suffix < 10u)
			{
				candidate.append("00");
			}
			else if (suffix < 100u)
			{
				candidate.push_back('0');
			}
		}
		candidate += std::to_string(suffix);

		if (!nameExists(candidate))
		{
			return candidate;
		}
	}
}

void World::RemoveNameComponentInternal(virasa::ecs::Entity entity)
{
	if (!HasNameComponent(entity))
		return;

	const uint32_t denseIdx = _nameSparse[entity.index];
	const uint32_t lastIdx = static_cast<uint32_t>(_nameValues.size()) - 1u;

	if (denseIdx != lastIdx)
	{
		_nameValues[denseIdx] = std::move(_nameValues[lastIdx]);
		_nameEntities[denseIdx] = _nameEntities[lastIdx];
		_nameSparse[_nameEntities[denseIdx].index] = denseIdx;
	}

	_nameValues.pop_back();
	_nameEntities.pop_back();
	_nameSparse[entity.index] = kSparseNone;
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
	RemoveNameComponentInternal(entity);
	RemoveTransformComponentInternal(entity);
	RemoveMeshComponentInternal(entity);
	RemoveVisualComponentInternal(entity);
	RemoveDirectionalLightComponentInternal(entity);
	RemovePointLightComponentInternal(entity);
	RemoveSpotLightComponentInternal(entity);
	RemoveCameraComponentInternal(entity);

	// (b) Hierarchy detachment
	if (detachFromParent)
	{
		virasa::ecs::Entity parentEntity = _parents[entity.index];
		if (parentEntity != virasa::ecs::Entity::Invalid())
		{
			auto& parentChildren = _children[parentEntity.index];
			RemoveFromChildren(parentChildren, entity);
		}
		else
		{
			RemoveFromRoots(entity);
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
	, _roots(std::move(other._roots))
	, _entityCount(other._entityCount)
	, _nameValues(std::move(other._nameValues))
	, _nameEntities(std::move(other._nameEntities))
	, _nameSparse(std::move(other._nameSparse))
	, _transformValues(std::move(other._transformValues))
	, _transformEntities(std::move(other._transformEntities))
	, _transformSparse(std::move(other._transformSparse))
	, _meshValues(std::move(other._meshValues))
	, _meshEntities(std::move(other._meshEntities))
	, _meshSparse(std::move(other._meshSparse))
	, _visualValues(std::move(other._visualValues))
	, _visualEntities(std::move(other._visualEntities))
	, _visualSparse(std::move(other._visualSparse))
	, _directionalLightValues(std::move(other._directionalLightValues))
	, _directionalLightEntities(std::move(other._directionalLightEntities))
	, _directionalLightSparse(std::move(other._directionalLightSparse))
	, _pointLightValues(std::move(other._pointLightValues))
	, _pointLightEntities(std::move(other._pointLightEntities))
	, _pointLightSparse(std::move(other._pointLightSparse))
	, _spotLightValues(std::move(other._spotLightValues))
	, _spotLightEntities(std::move(other._spotLightEntities))
	, _spotLightSparse(std::move(other._spotLightSparse))
	, _cameraValues(std::move(other._cameraValues))
	, _cameraEntities(std::move(other._cameraEntities))
	, _cameraSparse(std::move(other._cameraSparse))
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
		_roots             = std::move(other._roots);
		_parents           = std::move(other._parents);
		_children          = std::move(other._children);
		_entityCount       = other._entityCount;
		_nameValues        = std::move(other._nameValues);
		_nameEntities      = std::move(other._nameEntities);
		_nameSparse        = std::move(other._nameSparse);
		_transformValues   = std::move(other._transformValues);
		_transformEntities = std::move(other._transformEntities);
		_transformSparse   = std::move(other._transformSparse);
		_meshValues        = std::move(other._meshValues);
		_meshEntities      = std::move(other._meshEntities);
		_meshSparse        = std::move(other._meshSparse);
		_visualValues               = std::move(other._visualValues);
		_visualEntities             = std::move(other._visualEntities);
		_visualSparse               = std::move(other._visualSparse);
		_directionalLightValues     = std::move(other._directionalLightValues);
		_directionalLightEntities   = std::move(other._directionalLightEntities);
		_directionalLightSparse     = std::move(other._directionalLightSparse);
		_pointLightValues           = std::move(other._pointLightValues);
		_pointLightEntities         = std::move(other._pointLightEntities);
		_pointLightSparse           = std::move(other._pointLightSparse);
		_spotLightValues            = std::move(other._spotLightValues);
		_spotLightEntities          = std::move(other._spotLightEntities);
		_spotLightSparse            = std::move(other._spotLightSparse);
		_cameraValues               = std::move(other._cameraValues);
		_cameraEntities             = std::move(other._cameraEntities);
		_cameraSparse               = std::move(other._cameraSparse);
		other._entityCount          = 0u;
	}
	return *this;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

virasa::ecs::Entity World::CreateEntity(std::string_view name)
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

	virasa::ecs::Entity entity;
	entity.index = slotIndex;
	entity.generation = generation;

	EnsureSparseCapacity(_nameSparse, entity.index);
	const uint32_t denseIdx = static_cast<uint32_t>(_nameValues.size());
	_nameValues.push_back(virasa::ecs::NameComponent{MakeUniqueEntityName(name)});
	_nameEntities.push_back(entity);
	_nameSparse[entity.index] = denseIdx;

	_roots.push_back(entity);
	++_entityCount;
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
	if (child == virasa::ecs::Entity::Invalid() || !IsValid(child))
		return virasa::ecs::EcsError::InvalidEntity;

	if (parent != virasa::ecs::Entity::Invalid() && !IsValid(parent))
		return virasa::ecs::EcsError::InvalidEntity;

	if (parent == child)
		return virasa::ecs::EcsError::SelfParent;

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

	virasa::ecs::Entity currentParent = _parents[child.index];
	if (currentParent == parent)
	{
		return virasa::ecs::EcsError::None;
	}

	if (currentParent != virasa::ecs::Entity::Invalid())
	{
		auto& siblings = _children[currentParent.index];
		RemoveFromChildren(siblings, child);
	}
	else if (parent != virasa::ecs::Entity::Invalid())
	{
		RemoveFromRoots(child);
	}

	_parents[child.index] = parent;

	if (parent != virasa::ecs::Entity::Invalid())
	{
		_children[parent.index].push_back(child);
	}
	else if (currentParent != virasa::ecs::Entity::Invalid())
	{
		_roots.push_back(child);
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

const std::vector<virasa::ecs::Entity>& World::GetRoots() const noexcept
{
	return _roots;
}

virasa::ecs::Entity World::FindEntityByName(std::string_view name) const
{
	for (size_t i = 0; i < _nameEntities.size(); ++i)
	{
		if (_nameValues[i].name == name)
		{
			return _nameEntities[i];
		}
	}

	return virasa::ecs::Entity::Invalid();
}

bool World::HasNameComponent(virasa::ecs::Entity entity) const noexcept
{
	if (!IsValid(entity))
		return false;
	if (entity.index >= static_cast<uint32_t>(_nameSparse.size()))
		return false;
	const uint32_t denseIdx = _nameSparse[entity.index];
	if (denseIdx == kSparseNone)
		return false;
	return _nameEntities[denseIdx] == entity;
}

const virasa::ecs::NameComponent& World::GetNameComponent(virasa::ecs::Entity entity) const
{
	return _nameValues[_nameSparse[entity.index]];
}

const std::vector<virasa::ecs::Entity>& World::GetNameComponentEntities() const noexcept
{
	return _nameEntities;
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

// ---------------------------------------------------------------------------
// DirectionalLightComponent
// ---------------------------------------------------------------------------

void World::RemoveDirectionalLightComponentInternal(virasa::ecs::Entity entity)
{
	if (!HasDirectionalLightComponent(entity))
		return;

	const uint32_t denseIdx = _directionalLightSparse[entity.index];
	const uint32_t lastIdx  = static_cast<uint32_t>(_directionalLightValues.size()) - 1u;

	if (denseIdx != lastIdx)
	{
		_directionalLightValues[denseIdx]   = _directionalLightValues[lastIdx];
		_directionalLightEntities[denseIdx] = _directionalLightEntities[lastIdx];
		_directionalLightSparse[_directionalLightEntities[denseIdx].index] = denseIdx;
	}

	_directionalLightValues.pop_back();
	_directionalLightEntities.pop_back();
	_directionalLightSparse[entity.index] = kSparseNone;
}

void World::AddDirectionalLightComponent(virasa::ecs::Entity entity, virasa::ecs::DirectionalLightComponent component)
{
	EnsureSparseCapacity(_directionalLightSparse, entity.index);

	const uint32_t denseIdx = static_cast<uint32_t>(_directionalLightValues.size());
	_directionalLightValues.push_back(component);
	_directionalLightEntities.push_back(entity);
	_directionalLightSparse[entity.index] = denseIdx;
}

void World::RemoveDirectionalLightComponent(virasa::ecs::Entity entity)
{
	RemoveDirectionalLightComponentInternal(entity);
}

bool World::HasDirectionalLightComponent(virasa::ecs::Entity entity) const noexcept
{
	if (!IsValid(entity))
		return false;
	if (entity.index >= static_cast<uint32_t>(_directionalLightSparse.size()))
		return false;
	const uint32_t denseIdx = _directionalLightSparse[entity.index];
	if (denseIdx == kSparseNone)
		return false;
	return _directionalLightEntities[denseIdx] == entity;
}

const virasa::ecs::DirectionalLightComponent& World::GetDirectionalLightComponent(virasa::ecs::Entity entity) const
{
	return _directionalLightValues[_directionalLightSparse[entity.index]];
}

virasa::ecs::DirectionalLightComponent& World::GetDirectionalLightComponent(virasa::ecs::Entity entity)
{
	return _directionalLightValues[_directionalLightSparse[entity.index]];
}

const std::vector<virasa::ecs::Entity>& World::GetDirectionalLightComponentEntities() const noexcept
{
	return _directionalLightEntities;
}

// ---------------------------------------------------------------------------
// PointLightComponent
// ---------------------------------------------------------------------------

void World::RemovePointLightComponentInternal(virasa::ecs::Entity entity)
{
	if (!HasPointLightComponent(entity))
		return;

	const uint32_t denseIdx = _pointLightSparse[entity.index];
	const uint32_t lastIdx  = static_cast<uint32_t>(_pointLightValues.size()) - 1u;

	if (denseIdx != lastIdx)
	{
		_pointLightValues[denseIdx]   = _pointLightValues[lastIdx];
		_pointLightEntities[denseIdx] = _pointLightEntities[lastIdx];
		_pointLightSparse[_pointLightEntities[denseIdx].index] = denseIdx;
	}

	_pointLightValues.pop_back();
	_pointLightEntities.pop_back();
	_pointLightSparse[entity.index] = kSparseNone;
}

void World::AddPointLightComponent(virasa::ecs::Entity entity, virasa::ecs::PointLightComponent component)
{
	EnsureSparseCapacity(_pointLightSparse, entity.index);

	const uint32_t denseIdx = static_cast<uint32_t>(_pointLightValues.size());
	_pointLightValues.push_back(component);
	_pointLightEntities.push_back(entity);
	_pointLightSparse[entity.index] = denseIdx;
}

void World::RemovePointLightComponent(virasa::ecs::Entity entity)
{
	RemovePointLightComponentInternal(entity);
}

bool World::HasPointLightComponent(virasa::ecs::Entity entity) const noexcept
{
	if (!IsValid(entity))
		return false;
	if (entity.index >= static_cast<uint32_t>(_pointLightSparse.size()))
		return false;
	const uint32_t denseIdx = _pointLightSparse[entity.index];
	if (denseIdx == kSparseNone)
		return false;
	return _pointLightEntities[denseIdx] == entity;
}

const virasa::ecs::PointLightComponent& World::GetPointLightComponent(virasa::ecs::Entity entity) const
{
	return _pointLightValues[_pointLightSparse[entity.index]];
}

virasa::ecs::PointLightComponent& World::GetPointLightComponent(virasa::ecs::Entity entity)
{
	return _pointLightValues[_pointLightSparse[entity.index]];
}

const std::vector<virasa::ecs::Entity>& World::GetPointLightComponentEntities() const noexcept
{
	return _pointLightEntities;
}

// ---------------------------------------------------------------------------
// SpotLightComponent
// ---------------------------------------------------------------------------

void World::RemoveSpotLightComponentInternal(virasa::ecs::Entity entity)
{
	if (!HasSpotLightComponent(entity))
		return;

	const uint32_t denseIdx = _spotLightSparse[entity.index];
	const uint32_t lastIdx  = static_cast<uint32_t>(_spotLightValues.size()) - 1u;

	if (denseIdx != lastIdx)
	{
		_spotLightValues[denseIdx]   = _spotLightValues[lastIdx];
		_spotLightEntities[denseIdx] = _spotLightEntities[lastIdx];
		_spotLightSparse[_spotLightEntities[denseIdx].index] = denseIdx;
	}

	_spotLightValues.pop_back();
	_spotLightEntities.pop_back();
	_spotLightSparse[entity.index] = kSparseNone;
}

void World::AddSpotLightComponent(virasa::ecs::Entity entity, virasa::ecs::SpotLightComponent component)
{
	EnsureSparseCapacity(_spotLightSparse, entity.index);

	const uint32_t denseIdx = static_cast<uint32_t>(_spotLightValues.size());
	_spotLightValues.push_back(component);
	_spotLightEntities.push_back(entity);
	_spotLightSparse[entity.index] = denseIdx;
}

void World::RemoveSpotLightComponent(virasa::ecs::Entity entity)
{
	RemoveSpotLightComponentInternal(entity);
}

bool World::HasSpotLightComponent(virasa::ecs::Entity entity) const noexcept
{
	if (!IsValid(entity))
		return false;
	if (entity.index >= static_cast<uint32_t>(_spotLightSparse.size()))
		return false;
	const uint32_t denseIdx = _spotLightSparse[entity.index];
	if (denseIdx == kSparseNone)
		return false;
	return _spotLightEntities[denseIdx] == entity;
}

const virasa::ecs::SpotLightComponent& World::GetSpotLightComponent(virasa::ecs::Entity entity) const
{
	return _spotLightValues[_spotLightSparse[entity.index]];
}

virasa::ecs::SpotLightComponent& World::GetSpotLightComponent(virasa::ecs::Entity entity)
{
	return _spotLightValues[_spotLightSparse[entity.index]];
}

const std::vector<virasa::ecs::Entity>& World::GetSpotLightComponentEntities() const noexcept
{
	return _spotLightEntities;
}

// ---------------------------------------------------------------------------
// CameraComponent
// ---------------------------------------------------------------------------

void World::RemoveCameraComponentInternal(virasa::ecs::Entity entity)
{
	if (!HasCameraComponent(entity))
		return;

	const uint32_t denseIdx = _cameraSparse[entity.index];
	const uint32_t lastIdx  = static_cast<uint32_t>(_cameraValues.size()) - 1u;

	if (denseIdx != lastIdx)
	{
		_cameraValues[denseIdx]   = _cameraValues[lastIdx];
		_cameraEntities[denseIdx] = _cameraEntities[lastIdx];
		_cameraSparse[_cameraEntities[denseIdx].index] = denseIdx;
	}

	_cameraValues.pop_back();
	_cameraEntities.pop_back();
	_cameraSparse[entity.index] = kSparseNone;
}

void World::AddCameraComponent(virasa::ecs::Entity entity, virasa::ecs::CameraComponent component)
{
	EnsureSparseCapacity(_cameraSparse, entity.index);

	const uint32_t denseIdx = static_cast<uint32_t>(_cameraValues.size());
	_cameraValues.push_back(component);
	_cameraEntities.push_back(entity);
	_cameraSparse[entity.index] = denseIdx;
}

void World::RemoveCameraComponent(virasa::ecs::Entity entity)
{
	RemoveCameraComponentInternal(entity);
}

bool World::HasCameraComponent(virasa::ecs::Entity entity) const noexcept
{
	if (!IsValid(entity))
		return false;
	if (entity.index >= static_cast<uint32_t>(_cameraSparse.size()))
		return false;
	const uint32_t denseIdx = _cameraSparse[entity.index];
	if (denseIdx == kSparseNone)
		return false;
	return _cameraEntities[denseIdx] == entity;
}

const virasa::ecs::CameraComponent& World::GetCameraComponent(virasa::ecs::Entity entity) const
{
	return _cameraValues[_cameraSparse[entity.index]];
}

virasa::ecs::CameraComponent& World::GetCameraComponent(virasa::ecs::Entity entity)
{
	return _cameraValues[_cameraSparse[entity.index]];
}

const std::vector<virasa::ecs::Entity>& World::GetCameraComponentEntities() const noexcept
{
	return _cameraEntities;
}

} // namespace virasa::ecs
