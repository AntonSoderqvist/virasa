#include "virasa/sim/behaviors/VehicleBehavior.h"

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/TransformSystem.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/input/Actions.h"
#include "virasa/input/StepBridge.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"
#include "virasa/physics/PhysicsComponents.h"
#include "virasa/physics/PhysicsWorld.h"
#include "virasa/sim/Tick.h"
#include "virasa/sim/VehicleComponent.h"
#include "virasa/sim/WheelComponent.h"

#include "glm/gtc/quaternion.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <typeindex>
#include <typeinfo>
#include <vector>

namespace
{

[[nodiscard]] float AxisOrNeutral(
	const virasa::input::ActionState* actions,
	virasa::input::ActionId action) noexcept
{
	return actions != nullptr ? actions->Axis(action) : 0.0f;
}

[[nodiscard]] bool HeldOrNeutral(
	const virasa::input::ActionState* actions,
	virasa::input::ActionId action) noexcept
{
	return actions != nullptr && actions->Held(action);
}

struct UsableWheel
{
	virasa::ecs::Entity entity = virasa::ecs::Entity::Invalid();
	virasa::sim::WheelComponent component{};
	virasa::math::Transform local{};
	virasa::math::Vec3 mount{0.0f, 0.0f, 0.0f};
	float contactRadius = 0.0f;
};

[[nodiscard]] float SurfaceFriction(
	const virasa::ecs::ComponentSystem* rigidBodySystem,
	virasa::ecs::Entity hitEntity) noexcept
{
	if (rigidBodySystem == nullptr || !rigidBodySystem->Has(hitEntity))
	{
		return 1.0f;
	}

	const auto* body = static_cast<const virasa::physics::RigidBodyComponent*>(
		rigidBodySystem->GetRaw(hitEntity));
	return body->friction;
}

[[nodiscard]] virasa::math::Vec3 RotatedAboutUp(
	const virasa::math::Vec3& direction,
	const virasa::math::Vec3& up,
	float angle)
{
	return glm::normalize(glm::angleAxis(angle, up) * direction);
}

[[nodiscard]] float ContactRadius(
	const virasa::math::Transform& local,
	const virasa::sim::WheelComponent& wheel) noexcept
{
	const float diameter = std::max({local.scale.x, local.scale.y, local.scale.z});
	return 0.5f * diameter * wheel.radiusFactor;
}

} // namespace

namespace virasa::sim::behaviors
{

const char* VehicleBehavior::Name() const noexcept
{
	return "Vehicle";
}

void VehicleBehavior::Step(
	virasa::ecs::World& world,
	const virasa::sim::TickContext& tick,
	virasa::ecs::CommandBuffer& commands)
{
	(void)commands;

	auto* physicsWorld = static_cast<virasa::physics::PhysicsWorld*>(
		world.GetResource(std::type_index(typeid(virasa::physics::PhysicsWorld))));
	if (physicsWorld == nullptr)
	{
		return;
	}

	const virasa::ecs::ComponentId vehicleId = world.GetSystemId("Vehicle");
	if (vehicleId == virasa::ecs::kInvalidComponentId)
	{
		return;
	}

	const virasa::ecs::ComponentId wheelId = world.GetSystemId("Wheel");
	if (wheelId == virasa::ecs::kInvalidComponentId)
	{
		return;
	}

	const virasa::ecs::ComponentId transformId = world.GetSystemId("Transform");
	if (transformId == virasa::ecs::kInvalidComponentId)
	{
		return;
	}

	virasa::ecs::ComponentSystem& vehicleSystem = world.GetSystem(vehicleId);
	virasa::ecs::ComponentSystem& wheelSystem = world.GetSystem(wheelId);
	auto& transformSystem = static_cast<virasa::ecs::TransformSystem&>(
		world.GetSystem(transformId));

	virasa::ecs::ComponentSystem* rigidBodySystem = nullptr;
	const virasa::ecs::ComponentId rigidBodyId = world.GetSystemId("RigidBody");
	if (rigidBodyId != virasa::ecs::kInvalidComponentId)
	{
		rigidBodySystem = &world.GetSystem(rigidBodyId);
	}

	const virasa::input::ActionState* actions =
		virasa::input::StepBridge::GetActions(world);
	const std::vector<virasa::ecs::Entity> entities = world.GetEntities({vehicleId});

	for (virasa::ecs::Entity entity : entities)
	{
		if (!physicsWorld->HasBody(entity))
		{
			continue;
		}

		const auto* vehicle = static_cast<const virasa::sim::VehicleComponent*>(
			vehicleSystem.GetRaw(entity));
		const virasa::math::Transform chassisPose = physicsWorld->GetBodyTransform(entity);

		const virasa::math::Vec3 right =
			glm::normalize(chassisPose.rotation * virasa::math::Vec3(1.0f, 0.0f, 0.0f));
		const virasa::math::Vec3 forward =
			glm::normalize(chassisPose.rotation * virasa::math::Vec3(0.0f, 1.0f, 0.0f));
		const virasa::math::Vec3 up =
			glm::normalize(chassisPose.rotation * virasa::math::Vec3(0.0f, 0.0f, 1.0f));
		const virasa::math::Vec3 down = -up;

		const float throttle = AxisOrNeutral(actions, vehicle->throttleAction);
		const float steeringAngle =
			vehicle->maxSteerAngle * AxisOrNeutral(actions, vehicle->steerAction);
		const bool braking = HeldOrNeutral(actions, vehicle->brakeAction);
		const bool handbrake = HeldOrNeutral(actions, vehicle->handbrakeAction);

		const std::array<virasa::ecs::Entity, 4> wheelEntities = {
			vehicle->wheels.frontRight,
			vehicle->wheels.frontLeft,
			vehicle->wheels.backRight,
			vehicle->wheels.backLeft};

		std::vector<UsableWheel> usableWheels;
		usableWheels.reserve(wheelEntities.size());
		int drivenWheelCount = 0;
		for (virasa::ecs::Entity wheelEntity : wheelEntities)
		{
			if (
				!wheelEntity.IsValid() ||
				!wheelSystem.Has(wheelEntity) ||
				!transformSystem.Has(wheelEntity))
			{
				continue;
			}

			const auto* wheel = static_cast<const virasa::sim::WheelComponent*>(
				wheelSystem.GetRaw(wheelEntity));
			const virasa::math::Transform local = transformSystem.GetLocal(wheelEntity);

			// The wheel entity's authored local translation is the fixed strut
			// mount. Because this behavior also writes the suspension-dropped
			// visual position back into the same local transform each step, the
			// mount must be cached on first encounter and reused, rather than
			// re-read from the (behavior-mutated) transform, or it would march
			// down every step and the suspension would saturate.
			auto mountIter = _wheelRestMounts.find(wheelEntity.index);
			if (mountIter == _wheelRestMounts.end())
			{
				mountIter =
					_wheelRestMounts.emplace(wheelEntity.index, local.translation).first;
			}

			usableWheels.push_back(UsableWheel{
				wheelEntity,
				*wheel,
				local,
				mountIter->second,
				ContactRadius(local, *wheel)});

			if (wheel->driven)
			{
				++drivenWheelCount;
			}
		}

		for (const UsableWheel& wheel : usableWheels)
		{
			const virasa::math::Vec3 mountLocal = wheel.mount;
			const virasa::math::Vec3 mountWorld =
				chassisPose.translation + chassisPose.rotation * mountLocal;
			const virasa::math::Vec3 wheelHeading = wheel.component.steered ?
				RotatedAboutUp(forward, up, steeringAngle) :
				forward;
			const virasa::math::Vec3 wheelRight = wheel.component.steered ?
				RotatedAboutUp(right, up, steeringAngle) :
				right;
			const virasa::physics::CastHit hit = physicsWorld->SphereCast(
				mountWorld,
				wheel.contactRadius,
				down,
				vehicle->suspensionRestLength,
				entity);

			float longitudinalSpeed = 0.0f;
			if (hit.hit)
			{
				const virasa::math::Vec3 pointVelocity =
					physicsWorld->GetPointVelocity(entity, mountWorld);
				const float compression =
					vehicle->suspensionRestLength -
					(hit.fraction * vehicle->suspensionRestLength);
				const float suspensionSpeed = glm::dot(pointVelocity, up);
				const float suspensionMagnitude = std::max(
					0.0f,
					(vehicle->suspensionStiffness * compression) -
						(vehicle->suspensionDamping * suspensionSpeed));
				physicsWorld->AddForceAtPoint(entity, up * suspensionMagnitude, hit.point);

				virasa::math::Vec3 longitudinalForce(0.0f, 0.0f, 0.0f);
				if (wheel.component.driven && drivenWheelCount > 0)
				{
					longitudinalForce +=
						wheelHeading * (vehicle->enginePower * throttle /
							static_cast<float>(drivenWheelCount));
				}

				longitudinalSpeed = glm::dot(pointVelocity, wheelHeading);
				if (braking || handbrake)
				{
					const float brakeMagnitude = std::clamp(
						longitudinalSpeed * vehicle->brakeForce,
						-vehicle->brakeForce,
						vehicle->brakeForce);
					longitudinalForce += -wheelHeading * brakeMagnitude;
				}

				if (glm::dot(longitudinalForce, longitudinalForce) > 0.0f)
				{
					physicsWorld->AddForceAtPoint(entity, longitudinalForce, hit.point);
				}

				const float friction = SurfaceFriction(rigidBodySystem, hit.entity);
				const float lateralSpeed = glm::dot(pointVelocity, wheelRight);

				// Clamp lateral grip to a friction-circle limit tied to the
				// wheel's current normal load. Unbounded grip grows with sideways
				// speed and exceeds what the contact can physically supply, which
				// is what rolls the chassis. friction acts as the surface mu.
				float lateralMagnitude = -lateralSpeed * vehicle->tireGrip * friction;
				const float gripLimit = friction * suspensionMagnitude;
				lateralMagnitude = std::clamp(lateralMagnitude, -gripLimit, gripLimit);
				const virasa::math::Vec3 lateralForce = wheelRight * lateralMagnitude;

				// Apply lateral grip above the contact patch, partway toward the
				// chassis centre-of-mass plane, to tame the rollover couple while
				// preserving body lean. 0.0 applies at the contact (flip-prone),
				// 1.0 applies through the COM plane (no roll).
				const float comHeight =
					glm::dot(chassisPose.translation - hit.point, up);
				const virasa::math::Vec3 rollCenter =
					hit.point + up * (vehicle->rollCoupleFactor * comHeight);
				physicsWorld->AddForceAtPoint(entity, lateralForce, rollCenter);

				constexpr float kRadiusEpsilon = 0.0001f;
				_wheelRollAngles[wheel.entity.index] +=
					(longitudinalSpeed / std::max(wheel.contactRadius, kRadiusEpsilon)) *
					tick.deltaTime;
			}

			const float drop = hit.hit ?
				hit.fraction * vehicle->suspensionRestLength :
				vehicle->suspensionRestLength;
			virasa::math::Transform visualLocal = wheel.local;
			visualLocal.translation =
				virasa::math::Vec3(mountLocal.x, mountLocal.y, mountLocal.z - drop);
			visualLocal.rotation =
				glm::angleAxis(
					wheel.component.steered ? steeringAngle : 0.0f,
					virasa::math::Vec3(0.0f, 0.0f, 1.0f)) *
				glm::angleAxis(
					_wheelRollAngles[wheel.entity.index],
					virasa::math::Vec3(1.0f, 0.0f, 0.0f));
			transformSystem.SetLocal(wheel.entity, visualLocal);
		}
	}
}

} // namespace virasa::sim::behaviors
