#include "virasa/ecs/World.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <unordered_map>

namespace virasa::ecs
{

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

World::World()
{
	// Register built-in systems in the pinned order:
	// 0 – Transform, 1 – Mesh, 2 – Visual, 3 – DirectionalLight,
	// 4 – PointLight, 5 – SpotLight, 6 – Camera

	auto transformSys = std::make_unique<TransformSystem>(0u);
	_transformSystem = transformSys.get();
	RegisterSystem(std::move(transformSys));

	RegisterSystem(std::make_unique<SparseComponentSystem>(
		1u, "Mesh", sizeof(MeshComponent)));
	RegisterSystem(std::make_unique<SparseComponentSystem>(
		2u, "Visual", sizeof(VisualComponent)));
	RegisterSystem(std::make_unique<SparseComponentSystem>(
		3u, "DirectionalLight", sizeof(DirectionalLightComponent)));
	RegisterSystem(std::make_unique<SparseComponentSystem>(
		4u, "PointLight", sizeof(PointLightComponent)));
	RegisterSystem(std::make_unique<SparseComponentSystem>(
		5u, "SpotLight", sizeof(SpotLightComponent)));
	RegisterSystem(std::make_unique<SparseComponentSystem>(
		6u, "Camera", sizeof(CameraComponent)));
}

World::World(World&& other) noexcept
	: _slots(std::move(other._slots))
	, _parents(std::move(other._parents))
	, _children(std::move(other._children))
	, _roots(std::move(other._roots))
	, _freeList(std::move(other._freeList))
	, _entityCount(other._entityCount)
	, _nameComponents(std::move(other._nameComponents))
	, _nameEntities(std::move(other._nameEntities))
	, _nameSparse(std::move(other._nameSparse))
	, _systems(std::move(other._systems))
	, _systemNameToId(std::move(other._systemNameToId))
	, _transformSystem(other._transformSystem)
{
	other._entityCount = 0u;
	other._transformSystem = nullptr;
}

World& World::operator=(World&& other) noexcept
{
	if (this == &other)
		return *this;

	// Tear down existing state (systems destroyed via unique_ptr)
	_systems.clear();
	_systemNameToId.clear();
	_slots.clear();
	_parents.clear();
	_children.clear();
	_roots.clear();
	_freeList.clear();
	_entityCount = 0u;
	_nameComponents.clear();
	_nameEntities.clear();
	_nameSparse.clear();
	_transformSystem = nullptr;

	// Take ownership
	_slots = std::move(other._slots);
	_parents = std::move(other._parents);
	_children = std::move(other._children);
	_roots = std::move(other._roots);
	_freeList = std::move(other._freeList);
	_entityCount = other._entityCount;
	_nameComponents = std::move(other._nameComponents);
	_nameEntities = std::move(other._nameEntities);
	_nameSparse = std::move(other._nameSparse);
	_systems = std::move(other._systems);
	_systemNameToId = std::move(other._systemNameToId);
	_transformSystem = other._transformSystem;

	other._entityCount = 0u;
	other._transformSystem = nullptr;

	return *this;
}

// ---------------------------------------------------------------------------
// Entity creation
// ---------------------------------------------------------------------------

Entity World::CreateEntity(std::string_view name)
{
	uint32_t index;
	uint32_t generation;

	if (!_freeList.empty())
	{
		index = _freeList.back();
		_freeList.pop_back();
		generation = _slots[index].generation;
		_slots[index].live = true;
		_parents[index] = Entity::Invalid();
		_children[index].clear();
	}
	else
	{
		index = static_cast<uint32_t>(_slots.size());
		assert(index != 0xFFFFFFFFu && "Entity index space exhausted");
		generation = 0u;
		_slots.push_back({ 0u, true });
		_parents.push_back(Entity::Invalid());
		_children.emplace_back();
	}

	Entity entity;
	entity.index = index;
	entity.generation = generation;

	++_entityCount;

	// Attach NameComponent
	std::string uniqueName = ResolveUniqueName(name);
	AddNameComponent(entity, std::move(uniqueName));

	// New entity has no parent → it is a root
	_roots.push_back(entity);

	return entity;
}

// ---------------------------------------------------------------------------
// Entity destruction
// ---------------------------------------------------------------------------

void World::DestroyEntityInternal(Entity entity, bool isRoot)
{
	uint32_t idx = entity.index;

	// Recurse into children first (depth-first)
	// Copy children list since we'll be modifying it
	std::vector<Entity> childrenCopy = _children[idx];
	for (Entity child : childrenCopy)
	{
		DestroyEntityInternal(child, false);
	}

	// (a) Remove NameComponent
	RemoveNameComponent(entity);

	// (a) Remove from all registered component systems
	for (auto& sys : _systems)
	{
		if (sys->Has(entity))
		{
			sys->Remove(entity);
		}
	}

	// (b) Hierarchy detachment
	Entity parent = _parents[idx];
	if (parent != Entity::Invalid())
	{
		// Remove from parent's children vector (swap-and-pop)
		auto& siblings = _children[parent.index];
		for (size_t i = 0; i < siblings.size(); ++i)
		{
			if (siblings[i] == entity)
			{
				siblings[i] = siblings.back();
				siblings.pop_back();
				break;
			}
		}
	}
	else if (isRoot)
	{
		// Remove from roots vector (swap-and-pop)
		for (size_t i = 0; i < _roots.size(); ++i)
		{
			if (_roots[i] == entity)
			{
				_roots[i] = _roots.back();
				_roots.pop_back();
				break;
			}
		}
	}

	_children[idx].clear();
	_parents[idx] = Entity::Invalid();

	// (c) Generation bump
	_slots[idx].generation++;
	_slots[idx].live = false;

	// (d) Free-list push
	_freeList.push_back(idx);

	--_entityCount;
}

void World::DestroyEntity(Entity entity)
{
	assert(IsValid(entity));
	DestroyEntityInternal(entity, true);
}

// ---------------------------------------------------------------------------
// IsValid
// ---------------------------------------------------------------------------

bool World::IsValid(Entity entity) const noexcept
{
	if (entity.index >= static_cast<uint32_t>(_slots.size()))
		return false;
	const Slot& slot = _slots[entity.index];
	return slot.live && slot.generation == entity.generation;
}

// ---------------------------------------------------------------------------
// Reserve
// ---------------------------------------------------------------------------

void World::Reserve(uint32_t entityCapacity)
{
	_slots.reserve(entityCapacity);
	_parents.reserve(entityCapacity);
	_children.reserve(entityCapacity);
}

// ---------------------------------------------------------------------------
// GetEntityCount
// ---------------------------------------------------------------------------

uint32_t World::GetEntityCount() const noexcept
{
	return _entityCount;
}

// ---------------------------------------------------------------------------
// Hierarchy
// ---------------------------------------------------------------------------

EcsError World::SetParent(Entity child, Entity parent)
{
	// (1) child must be valid
	if (child == Entity::Invalid() || !IsValid(child))
		return EcsError::InvalidEntity;

	// (2) parent must be valid or Invalid()
	if (parent != Entity::Invalid() && !IsValid(parent))
		return EcsError::InvalidEntity;

	// (3) self-parent
	if (parent == child)
		return EcsError::SelfParent;

	// (4) cycle detection: walk up from parent
	if (parent != Entity::Invalid())
	{
		Entity ancestor = parent;
		while (ancestor != Entity::Invalid())
		{
			if (ancestor == child)
				return EcsError::CycleDetected;
			ancestor = _parents[ancestor.index];
		}
	}

	// (5) Perform the reparent
	Entity oldParent = _parents[child.index];

	// Detach from old parent
	if (oldParent != Entity::Invalid())
	{
		auto& siblings = _children[oldParent.index];
		for (size_t i = 0; i < siblings.size(); ++i)
		{
			if (siblings[i] == child)
			{
				siblings[i] = siblings.back();
				siblings.pop_back();
				break;
			}
		}
	}

	// Update parent entry
	_parents[child.index] = parent;

	// Attach to new parent
	if (parent != Entity::Invalid())
	{
		_children[parent.index].push_back(child);
	}

	// Roots maintenance
	bool wasRoot = (oldParent == Entity::Invalid());
	bool isRoot  = (parent == Entity::Invalid());

	if (wasRoot && !isRoot)
	{
		// Remove from roots
		for (size_t i = 0; i < _roots.size(); ++i)
		{
			if (_roots[i] == child)
			{
				_roots[i] = _roots.back();
				_roots.pop_back();
				break;
			}
		}
	}
	else if (!wasRoot && isRoot)
	{
		_roots.push_back(child);
	}

	return EcsError::None;
}

Entity World::GetParent(Entity entity) const noexcept
{
	return _parents[entity.index];
}

const std::vector<Entity>& World::GetChildren(Entity entity) const
{
	return _children[entity.index];
}

const std::vector<Entity>& World::GetRoots() const noexcept
{
	return _roots;
}

// ---------------------------------------------------------------------------
// Name component storage helpers
// ---------------------------------------------------------------------------

std::string World::ResolveUniqueName(std::string_view name) const
{
	std::string baseName = name.empty() ? std::string("Entity") : std::string(name);

	// Check if baseName is already unique
	bool found = false;
	for (const auto& nc : _nameComponents)
	{
		if (nc.name == baseName)
		{
			found = true;
			break;
		}
	}
	if (!found)
		return baseName;

	// Try suffixes .001, .002, ...
	for (uint32_t suffix = 1; ; ++suffix)
	{
		std::string candidate;
		if (suffix < 1000u)
		{
			char buf[8];
			std::snprintf(buf, sizeof(buf), "%03u", suffix);
			candidate = baseName + "." + buf;
		}
		else
		{
			candidate = baseName + "." + std::to_string(suffix);
		}

		bool collision = false;
		for (const auto& nc : _nameComponents)
		{
			if (nc.name == candidate)
			{
				collision = true;
				break;
			}
		}
		if (!collision)
			return candidate;
	}
}

void World::AddNameComponent(Entity entity, std::string uniqueName)
{
	// Grow sparse vector if needed
	if (entity.index >= static_cast<uint32_t>(_nameSparse.size()))
		_nameSparse.resize(entity.index + 1u, 0xFFFFFFFFu);

	uint32_t denseIndex = static_cast<uint32_t>(_nameComponents.size());
	_nameSparse[entity.index] = denseIndex;

	NameComponent nc;
	nc.name = std::move(uniqueName);
	_nameComponents.push_back(std::move(nc));
	_nameEntities.push_back(entity);
}

void World::RemoveNameComponent(Entity entity)
{
	if (entity.index >= static_cast<uint32_t>(_nameSparse.size()))
		return;
	uint32_t denseIdx = _nameSparse[entity.index];
	if (denseIdx == 0xFFFFFFFFu)
		return;

	uint32_t lastIdx = static_cast<uint32_t>(_nameComponents.size()) - 1u;
	if (denseIdx != lastIdx)
	{
		// Swap with last
		_nameComponents[denseIdx] = std::move(_nameComponents[lastIdx]);
		_nameEntities[denseIdx] = _nameEntities[lastIdx];
		// Update sparse for swapped entity
		_nameSparse[_nameEntities[denseIdx].index] = denseIdx;
	}

	_nameComponents.pop_back();
	_nameEntities.pop_back();
	_nameSparse[entity.index] = 0xFFFFFFFFu;
}

// ---------------------------------------------------------------------------
// Name component accessors
// ---------------------------------------------------------------------------

Entity World::FindEntityByName(std::string_view name) const
{
	for (size_t i = 0; i < _nameEntities.size(); ++i)
	{
		if (_nameComponents[i].name == name)
			return _nameEntities[i];
	}
	return Entity::Invalid();
}

bool World::HasNameComponent(Entity entity) const noexcept
{
	if (!IsValid(entity))
		return false;
	if (entity.index >= static_cast<uint32_t>(_nameSparse.size()))
		return false;
	uint32_t denseIdx = _nameSparse[entity.index];
	if (denseIdx == 0xFFFFFFFFu)
		return false;
	return _nameEntities[denseIdx] == entity;
}

const NameComponent& World::GetNameComponent(Entity entity) const
{
	assert(HasNameComponent(entity));
	return _nameComponents[_nameSparse[entity.index]];
}

const std::vector<Entity>& World::GetNameComponentEntities() const noexcept
{
	return _nameEntities;
}

// ---------------------------------------------------------------------------
// System registry
// ---------------------------------------------------------------------------

ComponentId World::RegisterSystem(std::unique_ptr<ComponentSystem> system)
{
	assert(system != nullptr);
	ComponentId id = static_cast<ComponentId>(_systems.size());
	// Per the contract, RegisterSystem assigns the next dense id and writes it
	// into the system so its own Id() returns the assigned value.
	system->SetId(id);
	_systemNameToId.emplace_back(system->Name(), id);
	_systems.push_back(std::move(system));
	return id;
}

ComponentId World::GetSystemId(std::string_view name) const noexcept
{
	for (const auto& [sysName, sysId] : _systemNameToId)
	{
		if (sysName == name)
			return sysId;
	}
	return kInvalidComponentId;
}

ComponentSystem& World::GetSystem(ComponentId id)
{
	assert(id < static_cast<ComponentId>(_systems.size()));
	return *_systems[id];
}

const ComponentSystem& World::GetSystem(ComponentId id) const
{
	assert(id < static_cast<ComponentId>(_systems.size()));
	return *_systems[id];
}

ComponentSystem* World::FindSystem(std::string_view name) noexcept
{
	ComponentId id = GetSystemId(name);
	if (id == kInvalidComponentId)
		return nullptr;
	return _systems[id].get();
}

// ---------------------------------------------------------------------------
// GetEntities
// ---------------------------------------------------------------------------

std::vector<Entity> World::GetEntities(
	std::initializer_list<ComponentId> components) const
{
	if (components.size() == 0)
		return {};

	// Find the system with the smallest Size()
	const ComponentSystem* smallest = nullptr;
	for (ComponentId id : components)
	{
		const ComponentSystem* sys = _systems[id].get();
		if (smallest == nullptr || sys->Size() < smallest->Size())
			smallest = sys;
	}

	std::vector<Entity> result;
	result.reserve(smallest->Size());

	for (const Entity& entity : smallest->Entities())
	{
		bool hasAll = true;
		for (ComponentId id : components)
		{
			if (!_systems[id]->Has(entity))
			{
				hasAll = false;
				break;
			}
		}
		if (hasAll)
			result.push_back(entity);
	}

	return result;
}

// ---------------------------------------------------------------------------
// Transform accessors
// ---------------------------------------------------------------------------

TransformSystem& World::Transforms() noexcept
{
	return *_transformSystem;
}

const TransformSystem& World::GetTransforms() const noexcept
{
	return *_transformSystem;
}

// ---------------------------------------------------------------------------
// UpdateTransforms
// ---------------------------------------------------------------------------

void World::UpdateTransformsRecursive(
	Entity entity,
	const virasa::math::Mat4* parentWorld)
{
	if (!_transformSystem->Has(entity))
		return;

	virasa::math::Mat4 world;
	if (parentWorld == nullptr)
	{
		world = _transformSystem->GetLocal(entity).ToMatrix();
	}
	else
	{
		world = (*parentWorld) * _transformSystem->GetLocal(entity).ToMatrix();
	}
	_transformSystem->SetWorld(entity, world);

	for (const Entity& child : _children[entity.index])
	{
		UpdateTransformsRecursive(child, &world);
	}
}

void World::UpdateTransforms()
{
	// Collect the set of dirty entities
	const std::vector<Entity>& dirtyVec = _transformSystem->Dirty();
	if (dirtyVec.empty())
		return;

	// For each dirty entity, walk up to find the highest ancestor that is
	// also dirty or is the root of the affected subtree, then propagate
	// downward from there.
	// Strategy: collect all "subtree roots" — dirty entities whose parent
	// either has no transform or is not dirty.
	// We build a set of dirty entity indices for fast lookup.
	std::vector<uint32_t> dirtyIndices;
	dirtyIndices.reserve(dirtyVec.size());
	for (const Entity& e : dirtyVec)
		dirtyIndices.push_back(e.index);

	// Sort for binary search
	std::sort(dirtyIndices.begin(), dirtyIndices.end());

	auto isDirty = [&](uint32_t idx) -> bool
	{
		return std::binary_search(dirtyIndices.begin(), dirtyIndices.end(), idx);
	};

	// Find subtree roots: dirty entities whose parent either doesn't have a
	// transform or is not dirty.
	std::vector<Entity> subtreeRoots;
	for (const Entity& e : dirtyVec)
	{
		Entity parent = _parents[e.index];
		bool parentDirty = false;
		if (parent != Entity::Invalid() && _transformSystem->Has(parent))
		{
			parentDirty = isDirty(parent.index);
		}
		if (!parentDirty)
			subtreeRoots.push_back(e);
	}

	// For each subtree root, propagate downward
	for (const Entity& root : subtreeRoots)
	{
		Entity parent = _parents[root.index];
		if (parent != Entity::Invalid() && _transformSystem->Has(parent))
		{
			const virasa::math::Mat4& parentWorld = _transformSystem->GetWorld(parent);
			UpdateTransformsRecursive(root, &parentWorld);
		}
		else
		{
			UpdateTransformsRecursive(root, nullptr);
		}
	}

	_transformSystem->ClearAllDirty();
}

} // namespace virasa::ecs
