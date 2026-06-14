#include "virasa/sim/Builtins.h"

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/World.h"
#include "virasa/sim/BehaviorRegistry.h"
#include "virasa/sim/GameplayComponents.h"
#include "virasa/sim/behaviors/SpinBehavior.h"

#include <memory>

namespace virasa::sim
{

void RegisterGameplayComponents(virasa::ecs::World& world)
{
	if (world.GetSystemId("Spin") != virasa::ecs::kInvalidComponentId)
	{
		return;
	}

	(void)world.RegisterSystem(std::make_unique<virasa::ecs::SparseComponentSystem>(
		virasa::ecs::kInvalidComponentId,
		"Spin",
		sizeof(virasa::sim::SpinComponent)));
}

void RegisterBuiltinBehaviors(virasa::sim::BehaviorRegistry& registry)
{
	registry.Register("Spin", virasa::ecs::Phase::Step, []()
	{
		return std::make_unique<virasa::sim::behaviors::SpinBehavior>();
	});
}

} // namespace virasa::sim
