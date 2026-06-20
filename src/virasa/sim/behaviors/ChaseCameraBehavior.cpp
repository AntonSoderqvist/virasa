#include "virasa/sim/behaviors/ChaseCameraBehavior.h"

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/TransformSystem.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"
#include "virasa/sim/Tick.h"
#include "virasa/sim/VehicleComponent.h"

#include "glm/gtc/quaternion.hpp"

#include <cmath>
#include <vector>

namespace virasa::sim::behaviors
{
namespace
{
constexpr float kDegenerateLengthSquared = 1.0e-6f;
constexpr virasa::math::Vec3 kWorldUp(0.0f, 0.0f, 1.0f);

[[nodiscard]] virasa::math::Vec3 FlattenForward(const virasa::math::Transform& target) noexcept
{
	const virasa::math::Vec3 forward = target.Forward();
	virasa::math::Vec3 flattened(forward.x, forward.y, 0.0f);
	if (glm::dot(flattened, flattened) <= kDegenerateLengthSquared)
	{
		return forward;
	}
	return glm::normalize(flattened);
}

[[nodiscard]] float SmoothingFactor(float smoothing, float deltaTime) noexcept
{
	if (deltaTime <= 0.0f)
	{
		return 0.0f;
	}
	return 1.0f - std::exp(-smoothing * deltaTime);
}
} // namespace

const char* ChaseCameraBehavior::Name() const noexcept
{
	return "ChaseCamera";
}

void ChaseCameraBehavior::Step(
	virasa::ecs::World& world,
	const virasa::sim::TickContext& tick,
	virasa::ecs::CommandBuffer& commands)
{
	(void)commands;

	const virasa::ecs::ComponentId chaseCameraId = world.GetSystemId("ChaseCamera");
	const virasa::ecs::ComponentId vehicleId = world.GetSystemId("Vehicle");
	const virasa::ecs::ComponentId transformId = world.GetSystemId("Transform");
	if (chaseCameraId == virasa::ecs::kInvalidComponentId ||
		vehicleId == virasa::ecs::kInvalidComponentId ||
		transformId == virasa::ecs::kInvalidComponentId)
	{
		return;
	}

	const std::vector<virasa::ecs::Entity> targets = world.GetEntities({vehicleId, transformId});
	if (targets.empty())
	{
		return;
	}

	virasa::ecs::ComponentSystem& chaseCameraSystem = world.GetSystem(chaseCameraId);
	virasa::ecs::TransformSystem& transformSystem = world.Transforms();

	const virasa::ecs::Entity targetEntity = targets.front();
	const virasa::math::Transform targetTransform = transformSystem.GetLocal(targetEntity);
	const virasa::math::Vec3 targetPosition = targetTransform.translation;
	const virasa::math::Vec3 targetForward = FlattenForward(targetTransform);

	const std::vector<virasa::ecs::Entity> cameras =
		world.GetEntities({chaseCameraId, transformId});
	for (virasa::ecs::Entity cameraEntity : cameras)
	{
		const auto* chaseCamera = static_cast<const virasa::sim::behaviors::ChaseCameraComponent*>(
			chaseCameraSystem.GetRaw(cameraEntity));
		virasa::math::Transform cameraTransform = transformSystem.GetLocal(cameraEntity);

		const virasa::math::Vec3 desiredEye =
			targetPosition - targetForward * chaseCamera->distance + kWorldUp * chaseCamera->height;
		const float factor = SmoothingFactor(chaseCamera->positionSmoothing, tick.deltaTime);
		const virasa::math::Vec3 eye =
			cameraTransform.translation + (desiredEye - cameraTransform.translation) * factor;
		cameraTransform.translation = eye;

		const virasa::math::Vec3 aimPoint =
			targetPosition + kWorldUp * chaseCamera->lookAtHeight;
		const virasa::math::Vec3 toAim = aimPoint - eye;
		if (glm::dot(toAim, toAim) > kDegenerateLengthSquared)
		{
			const virasa::math::Vec3 forward = glm::normalize(toAim);
			const virasa::math::Vec3 rightCandidate = glm::cross(forward, kWorldUp);
			if (glm::dot(rightCandidate, rightCandidate) > kDegenerateLengthSquared)
			{
				const virasa::math::Vec3 right = glm::normalize(rightCandidate);
				const virasa::math::Vec3 up = glm::cross(right, forward);
				const virasa::math::Mat3 basis(right, forward, up);
				cameraTransform.rotation = glm::normalize(glm::quat_cast(basis));
			}
		}

		transformSystem.SetLocal(cameraEntity, cameraTransform);
	}
}

} // namespace virasa::sim::behaviors
