#include "virasa/sim/Builtins.h"

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/World.h"
#include "virasa/physics/PhysicsComponents.h"
#include "virasa/sim/BehaviorRegistry.h"
#include "virasa/sim/GameplayComponents.h"
#include "virasa/sim/behaviors/PhysicsBehavior.h"
#include "virasa/sim/behaviors/SpinBehavior.h"

#include <memory>

namespace virasa::sim
{

void RegisterGameplayComponents(virasa::ecs::World& world)
{
	if (world.GetSystemId("Spin") == virasa::ecs::kInvalidComponentId)
	{
		(void)world.RegisterSystem(std::make_unique<virasa::ecs::SparseComponentSystem>(
			virasa::ecs::kInvalidComponentId,
			"Spin",
			sizeof(virasa::sim::SpinComponent)));
	}

	if (world.GetSystemId("RigidBody") == virasa::ecs::kInvalidComponentId)
	{
		(void)world.RegisterSystem(std::make_unique<virasa::ecs::SparseComponentSystem>(
			virasa::ecs::kInvalidComponentId,
			"RigidBody",
			sizeof(virasa::physics::RigidBodyComponent)));
	}

	if (world.GetSystemId("Collider") == virasa::ecs::kInvalidComponentId)
	{
		(void)world.RegisterSystem(std::make_unique<virasa::ecs::SparseComponentSystem>(
			virasa::ecs::kInvalidComponentId,
			"Collider",
			sizeof(virasa::physics::ColliderComponent)));
	}
}

void RegisterBuiltinBehaviors(virasa::sim::BehaviorRegistry& registry)
{
	registry.Register("Spin", virasa::ecs::Phase::Step, []()
	{
		return std::make_unique<virasa::sim::behaviors::SpinBehavior>();
	});

	registry.Register("Physics", virasa::ecs::Phase::Step, []()
	{
		return std::make_unique<virasa::sim::behaviors::PhysicsBehavior>();
	});
}

} // namespace virasa::sim
