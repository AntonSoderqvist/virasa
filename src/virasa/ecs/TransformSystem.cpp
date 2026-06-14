#include "virasa/ecs/TransformSystem.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>

namespace virasa::ecs
{

static constexpr const char* kTransformComponentName = "Transform";

TransformSystem::TransformSystem(virasa::ecs::ComponentId id)
	: SparseComponentSystem(id, kTransformComponentName, sizeof(virasa::math::Transform))
{
}

void TransformSystem::Add(virasa::ecs::Entity entity, const virasa::math::Transform& local)
{
	AddRaw(entity, &local);
}

const virasa::math::Transform& TransformSystem::GetLocal(virasa::ecs::Entity entity) const
{
	const void* raw = GetRaw(entity);
	return *static_cast<const virasa::math::Transform*>(raw);
}

void TransformSystem::SetLocal(virasa::ecs::Entity entity, const virasa::math::Transform& local)
{
	SetRaw(entity, &local);
}

const virasa::math::Mat4& TransformSystem::GetWorld(virasa::ecs::Entity entity) const
{
	const void* raw = GetRaw(entity);
	// Determine the dense index by pointer arithmetic on the base's byte buffer.
	// We use the Entities() vector to find the dense index for this entity.
	const auto& entities = Entities();
	for (uint32_t i = 0; i < static_cast<uint32_t>(entities.size()); ++i)
	{
		if (entities[i].index == entity.index && entities[i].generation == entity.generation)
		{
			return _worldMatrices[i];
		}
	}
	// Should never reach here if entity is valid and has a component.
	return _worldMatrices[0];
}

void TransformSystem::SetWorld(virasa::ecs::Entity entity, const virasa::math::Mat4& world)
{
	const auto& entities = Entities();
	for (uint32_t i = 0; i < static_cast<uint32_t>(entities.size()); ++i)
	{
		if (entities[i].index == entity.index && entities[i].generation == entity.generation)
		{
			_worldMatrices[i] = world;
			return;
		}
	}
}

virasa::math::Vec3 TransformSystem::GetWorldPosition(virasa::ecs::Entity entity) const
{
	const virasa::math::Mat4& world = GetWorld(entity);
	// The translation is in the fourth column (column index 3).
	return virasa::math::Vec3(world[3][0], world[3][1], world[3][2]);
}

virasa::math::Vec3 TransformSystem::GetWorldForward(virasa::ecs::Entity entity) const
{
	const virasa::math::Mat4& world = GetWorld(entity);
	// Forward is the local +Y axis transformed by the world matrix's upper-left 3x3.
	virasa::math::Vec3 forward(
		world[1][0],
		world[1][1],
		world[1][2]
	);
	return glm::normalize(forward);
}

std::unique_ptr<virasa::ecs::ComponentSystem> TransformSystem::Clone() const
{
	auto clone = std::make_unique<virasa::ecs::TransformSystem>(Id());
	CopyStorageInto(*clone);
	clone->_worldMatrices = _worldMatrices;
	return clone;
}

void TransformSystem::OnAdd(uint32_t /*denseIndex*/)
{
	_worldMatrices.push_back(virasa::math::Mat4(1.0f));
}

void TransformSystem::OnRemoveSwap(uint32_t removedDenseIndex, uint32_t lastDenseIndex)
{
	_worldMatrices[removedDenseIndex] = _worldMatrices[lastDenseIndex];
	_worldMatrices.pop_back();
}

} // namespace virasa::ecs
