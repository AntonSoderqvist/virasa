#include "virasa/sim/Builtins.h"

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/World.h"
#include "virasa/physics/PhysicsComponents.h"
#include "virasa/sim/BehaviorRegistry.h"
#include "virasa/sim/GameplayComponents.h"
#include "virasa/sim/VehicleComponent.h"
#include "virasa/sim/WheelComponent.h"
#include "virasa/sim/behaviors/ChaseCameraBehavior.h"
#include "virasa/sim/behaviors/PhysicsBehavior.h"
#include "virasa/sim/behaviors/SpinBehavior.h"
#include "virasa/sim/behaviors/VehicleBehavior.h"

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

	if (world.GetSystemId("Vehicle") == virasa::ecs::kInvalidComponentId)
	{
		(void)world.RegisterSystem(std::make_unique<virasa::ecs::SparseComponentSystem>(
			virasa::ecs::kInvalidComponentId,
			"Vehicle",
			sizeof(virasa::sim::VehicleComponent)));
	}

	if (world.GetSystemId("Wheel") == virasa::ecs::kInvalidComponentId)
	{
		(void)world.RegisterSystem(std::make_unique<virasa::ecs::SparseComponentSystem>(
			virasa::ecs::kInvalidComponentId,
			"Wheel",
			sizeof(virasa::sim::WheelComponent)));
	}

	if (world.GetSystemId("ChaseCamera") == virasa::ecs::kInvalidComponentId)
	{
		(void)world.RegisterSystem(std::make_unique<virasa::ecs::SparseComponentSystem>(
			virasa::ecs::kInvalidComponentId,
			"ChaseCamera",
			sizeof(virasa::sim::behaviors::ChaseCameraComponent)));
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

	registry.Register("Vehicle", virasa::ecs::Phase::PreStep, []()
	{
		return std::make_unique<virasa::sim::behaviors::VehicleBehavior>();
	});

	registry.Register("ChaseCamera", virasa::ecs::Phase::PostStep, []()
	{
		return std::make_unique<virasa::sim::behaviors::ChaseCameraBehavior>();
	});
}

} // namespace virasa::sim
