#include "virasa/sim/behaviors/SpinBehavior.h"

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/TransformSystem.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"
#include "virasa/sim/GameplayComponents.h"
#include "virasa/sim/Tick.h"

#include "glm/gtc/quaternion.hpp"

#include <vector>

namespace virasa::sim::behaviors
{

const char* SpinBehavior::Name() const noexcept
{
	return "Spin";
}

void SpinBehavior::Step(
	virasa::ecs::World& world,
	const virasa::sim::TickContext& tick,
	virasa::ecs::CommandBuffer& commands)
{
	(void)commands;

	const virasa::ecs::ComponentId spinId = world.GetSystemId("Spin");
	const virasa::ecs::ComponentId transformId = world.GetSystemId("Transform");
	if (spinId == virasa::ecs::kInvalidComponentId ||
		transformId == virasa::ecs::kInvalidComponentId)
	{
		return;
	}

	virasa::ecs::ComponentSystem& spinSystem = world.GetSystem(spinId);
	virasa::ecs::TransformSystem& transformSystem = world.Transforms();
	const std::vector<virasa::ecs::Entity> entities = world.GetEntities({spinId, transformId});

	for (virasa::ecs::Entity entity : entities)
	{
		const auto* spin = static_cast<const virasa::sim::SpinComponent*>(
			spinSystem.GetRaw(entity));
		virasa::math::Transform local = transformSystem.GetLocal(entity);

		const float angularSpeed = glm::length(spin->angularVelocity);
		if (angularSpeed > 0.0f)
		{
			const virasa::math::Vec3 axis = spin->angularVelocity / angularSpeed;
			const virasa::math::Quat delta = glm::angleAxis(angularSpeed * tick.deltaTime, axis);
			local.rotation = glm::normalize(local.rotation * delta);
		}

		transformSystem.SetLocal(entity, local);
	}
}

} // namespace virasa::sim::behaviors
