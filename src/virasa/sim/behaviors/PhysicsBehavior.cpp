#include "virasa/sim/behaviors/PhysicsBehavior.h"

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/TransformSystem.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/math/Transform.h"
#include "virasa/physics/PhysicsComponents.h"
#include "virasa/physics/PhysicsWorld.h"
#include "virasa/sim/Tick.h"

#include <memory>
#include <typeindex>
#include <typeinfo>
#include <vector>

namespace virasa::sim::behaviors
{

PhysicsBehavior::PhysicsBehavior()
	: _physicsWorld(std::make_unique<virasa::physics::PhysicsWorld>(
		virasa::physics::PhysicsConfig{}))
{
}

PhysicsBehavior::~PhysicsBehavior() noexcept = default;

const char* PhysicsBehavior::Name() const noexcept
{
	return "Physics";
}

void PhysicsBehavior::Step(
	virasa::ecs::World& world,
	const virasa::sim::TickContext& tick,
	virasa::ecs::CommandBuffer& commands)
{
	(void)commands;

	world.SetResource(
		std::type_index(typeid(virasa::physics::PhysicsWorld)),
		_physicsWorld.get());

	if (!_populated)
	{
		const virasa::ecs::ComponentId rigidBodyId = world.GetSystemId("RigidBody");
		const virasa::ecs::ComponentId colliderId = world.GetSystemId("Collider");
		const virasa::ecs::ComponentId transformId = world.GetSystemId("Transform");
		if (rigidBodyId == virasa::ecs::kInvalidComponentId ||
			colliderId == virasa::ecs::kInvalidComponentId ||
			transformId == virasa::ecs::kInvalidComponentId)
		{
			return;
		}

		virasa::ecs::ComponentSystem& rigidBodySystem = world.GetSystem(rigidBodyId);
		virasa::ecs::ComponentSystem& colliderSystem = world.GetSystem(colliderId);
		virasa::ecs::TransformSystem& transformSystem = world.Transforms();
		const std::vector<virasa::ecs::Entity> entities =
			world.GetEntities({rigidBodyId, colliderId, transformId});

		for (virasa::ecs::Entity entity : entities)
		{
			const auto* body = static_cast<const virasa::physics::RigidBodyComponent*>(
				rigidBodySystem.GetRaw(entity));
			const auto* collider = static_cast<const virasa::physics::ColliderComponent*>(
				colliderSystem.GetRaw(entity));
			const virasa::math::Transform transform = transformSystem.GetLocal(entity);

			_physicsWorld->AddBody(entity, *body, *collider, transform);
		}

		_populated = true;
	}

	_physicsWorld->Step(tick.deltaTime);
	_physicsWorld->SyncToWorld(world);
}

} // namespace virasa::sim::behaviors
