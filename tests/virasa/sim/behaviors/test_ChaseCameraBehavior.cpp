#include <gtest/gtest.h>

#include "virasa/sim/behaviors/ChaseCameraBehavior.h"

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/TransformSystem.h"
#include "virasa/ecs/World.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"
#include "virasa/sim/Tick.h"
#include "virasa/sim/VehicleComponent.h"

#include "glm/geometric.hpp"
#include "glm/gtc/quaternion.hpp"

#include <memory>
#include <string>
#include <type_traits>

using namespace virasa::ecs;
using namespace virasa::math;
using namespace virasa::sim;
using namespace virasa::sim::behaviors;

namespace
{

constexpr float kEps = 1e-5f;
constexpr float kLooseTolerance = 0.05f;

void RegisterChaseCameraSystem(World& world)
{
	(void)world.RegisterSystem(std::make_unique<SparseComponentSystem>(
		kInvalidComponentId,
		"ChaseCamera",
		sizeof(ChaseCameraComponent)));
}

void RegisterVehicleSystem(World& world)
{
	(void)world.RegisterSystem(std::make_unique<SparseComponentSystem>(
		kInvalidComponentId,
		"Vehicle",
		sizeof(VehicleComponent)));
}

void AddChaseCameraComponent(World& world, Entity entity, const ChaseCameraComponent& chaseCamera)
{
	const ComponentId chaseCameraId = world.GetSystemId("ChaseCamera");
	ASSERT_NE(chaseCameraId, kInvalidComponentId);

	world.GetSystem(chaseCameraId).AddRaw(entity, &chaseCamera);
}

void AddVehicleComponent(World& world, Entity entity, const VehicleComponent& vehicle)
{
	const ComponentId vehicleId = world.GetSystemId("Vehicle");
	ASSERT_NE(vehicleId, kInvalidComponentId);

	world.GetSystem(vehicleId).AddRaw(entity, &vehicle);
}

void ExpectVec3Near(const Vec3& actual, const Vec3& expected, float tolerance)
{
	EXPECT_NEAR(actual.x, expected.x, tolerance);
	EXPECT_NEAR(actual.y, expected.y, tolerance);
	EXPECT_NEAR(actual.z, expected.z, tolerance);
}

void ExpectQuatNear(const Quat& actual, const Quat& expected, float tolerance)
{
	EXPECT_NEAR(actual.w, expected.w, tolerance);
	EXPECT_NEAR(actual.x, expected.x, tolerance);
	EXPECT_NEAR(actual.y, expected.y, tolerance);
	EXPECT_NEAR(actual.z, expected.z, tolerance);
}

void ExpectTransformUnchanged(const Transform& actual, const Transform& expected)
{
	ExpectVec3Near(actual.translation, expected.translation, kEps);
	ExpectQuatNear(actual.rotation, expected.rotation, kEps);
	ExpectVec3Near(actual.scale, expected.scale, kEps);
}

} // namespace

TEST(ChaseCameraBehavior, test_chase_camera_component_holds_follow_parameters)
{
	static_assert(std::is_copy_constructible_v<ChaseCameraComponent>);
	static_assert(std::is_copy_assignable_v<ChaseCameraComponent>);
	static_assert(std::is_move_constructible_v<ChaseCameraComponent>);
	static_assert(std::is_move_assignable_v<ChaseCameraComponent>);
	static_assert(std::is_default_constructible_v<ChaseCameraComponent>);
	static_assert(std::is_trivially_copyable_v<ChaseCameraComponent>);

	ChaseCameraComponent chaseCamera;

	EXPECT_FLOAT_EQ(chaseCamera.distance, 8.0f);
	EXPECT_FLOAT_EQ(chaseCamera.height, 3.0f);
	EXPECT_FLOAT_EQ(chaseCamera.lookAtHeight, 1.0f);
	EXPECT_FLOAT_EQ(chaseCamera.positionSmoothing, 8.0f);

	chaseCamera.distance = 12.0f;
	chaseCamera.height = 4.0f;
	chaseCamera.lookAtHeight = 2.0f;
	chaseCamera.positionSmoothing = 16.0f;

	ChaseCameraComponent copied = chaseCamera;
	EXPECT_FLOAT_EQ(copied.distance, 12.0f);
	EXPECT_FLOAT_EQ(copied.height, 4.0f);
	EXPECT_FLOAT_EQ(copied.lookAtHeight, 2.0f);
	EXPECT_FLOAT_EQ(copied.positionSmoothing, 16.0f);
}

TEST(ChaseCameraBehavior, test_chase_camera_behavior_is_concrete_camera_system)
{
	static_assert(std::is_final_v<ChaseCameraBehavior>);
	static_assert(std::is_base_of_v<Behavior, ChaseCameraBehavior>);
	static_assert(std::is_default_constructible_v<ChaseCameraBehavior>);
	static_assert(!std::is_copy_constructible_v<ChaseCameraBehavior>);
	static_assert(!std::is_copy_assignable_v<ChaseCameraBehavior>);
	static_assert(!std::is_move_constructible_v<ChaseCameraBehavior>);
	static_assert(!std::is_move_assignable_v<ChaseCameraBehavior>);
	static_assert(std::has_virtual_destructor_v<ChaseCameraBehavior>);

	std::unique_ptr<Behavior> behavior = std::make_unique<ChaseCameraBehavior>();
	EXPECT_EQ(std::string(behavior->Name()), "ChaseCamera");
	EXPECT_STREQ(behavior->Name(), "ChaseCamera");
}

TEST(ChaseCameraBehavior, test_chase_camera_behavior_follows_target_each_step)
{
	ChaseCameraBehavior behavior;
	TickContext tick;
	tick.deltaTime = 0.25f;

	World missingChaseCameraWorld;
	RegisterVehicleSystem(missingChaseCameraWorld);

	Entity missingChaseTarget = missingChaseCameraWorld.CreateEntity("MissingChaseTarget");
	Transform missingChaseTargetTransform;
	missingChaseTargetTransform.translation = Vec3(5.0f, 6.0f, 0.0f);
	missingChaseCameraWorld.Transforms().Add(missingChaseTarget, missingChaseTargetTransform);
	AddVehicleComponent(missingChaseCameraWorld, missingChaseTarget, VehicleComponent{});

	Entity missingChaseCamera = missingChaseCameraWorld.CreateEntity("MissingChaseCamera");
	Transform missingChaseCameraTransform;
	missingChaseCameraTransform.translation = Vec3(1.0f, 2.0f, 3.0f);
	missingChaseCameraWorld.Transforms().Add(missingChaseCamera, missingChaseCameraTransform);

	CommandBuffer missingChaseCommands;
	behavior.Step(missingChaseCameraWorld, tick, missingChaseCommands);

	EXPECT_TRUE(missingChaseCommands.IsEmpty());
	ExpectTransformUnchanged(
		missingChaseCameraWorld.Transforms().GetLocal(missingChaseCamera),
		missingChaseCameraTransform);

	World missingVehicleWorld;
	RegisterChaseCameraSystem(missingVehicleWorld);

	Entity missingVehicleCamera = missingVehicleWorld.CreateEntity("MissingVehicleCamera");
	Transform missingVehicleCameraTransform;
	missingVehicleCameraTransform.translation = Vec3(-1.0f, -2.0f, 3.0f);
	missingVehicleWorld.Transforms().Add(missingVehicleCamera, missingVehicleCameraTransform);
	AddChaseCameraComponent(missingVehicleWorld, missingVehicleCamera, ChaseCameraComponent{});

	CommandBuffer missingVehicleCommands;
	behavior.Step(missingVehicleWorld, tick, missingVehicleCommands);

	EXPECT_TRUE(missingVehicleCommands.IsEmpty());
	ExpectTransformUnchanged(
		missingVehicleWorld.Transforms().GetLocal(missingVehicleCamera),
		missingVehicleCameraTransform);

	World noTargetWorld;
	RegisterChaseCameraSystem(noTargetWorld);
	RegisterVehicleSystem(noTargetWorld);

	Entity noTargetCamera = noTargetWorld.CreateEntity("NoTargetCamera");
	Transform noTargetCameraTransform;
	noTargetCameraTransform.translation = Vec3(7.0f, 8.0f, 9.0f);
	noTargetWorld.Transforms().Add(noTargetCamera, noTargetCameraTransform);
	AddChaseCameraComponent(noTargetWorld, noTargetCamera, ChaseCameraComponent{});

	CommandBuffer noTargetCommands;
	behavior.Step(noTargetWorld, tick, noTargetCommands);

	EXPECT_TRUE(noTargetCommands.IsEmpty());
	ExpectTransformUnchanged(
		noTargetWorld.Transforms().GetLocal(noTargetCamera),
		noTargetCameraTransform);

	World world;
	RegisterChaseCameraSystem(world);
	RegisterVehicleSystem(world);

	Entity target = world.CreateEntity("VehicleTarget");
	Transform targetTransform;
	targetTransform.translation = Vec3(10.0f, 20.0f, 0.0f);
	targetTransform.rotation = glm::angleAxis(0.5f, Vec3(0.0f, 0.0f, 1.0f));
	world.Transforms().Add(target, targetTransform);
	AddVehicleComponent(world, target, VehicleComponent{});

	Entity camera = world.CreateEntity("ChaseCamera");
	Transform cameraTransform;
	cameraTransform.translation = Vec3(10.0f, 20.0f, 3.0f);
	cameraTransform.rotation = glm::angleAxis(0.25f, Vec3(0.0f, 0.0f, 1.0f));
	cameraTransform.scale = Vec3(2.0f, 3.0f, 4.0f);
	world.Transforms().Add(camera, cameraTransform);

	ChaseCameraComponent chaseCamera;
	chaseCamera.distance = 8.0f;
	chaseCamera.height = 3.0f;
	chaseCamera.lookAtHeight = 1.0f;
	chaseCamera.positionSmoothing = 8.0f;
	AddChaseCameraComponent(world, camera, chaseCamera);

	CommandBuffer commands;
	behavior.Step(world, tick, commands);

	const Vec3 targetForward = glm::normalize(targetTransform.rotation * Vec3(0.0f, 1.0f, 0.0f));
	const Vec3 desiredEye =
		targetTransform.translation - targetForward * chaseCamera.distance +
		Vec3(0.0f, 0.0f, chaseCamera.height);
	const Transform& updated = world.Transforms().GetLocal(camera);

	EXPECT_TRUE(commands.IsEmpty());
	EXPECT_LT(
		glm::length(updated.translation - desiredEye),
		glm::length(cameraTransform.translation - desiredEye));
	EXPECT_GT(glm::dot(updated.translation - cameraTransform.translation,
		desiredEye - cameraTransform.translation), 0.0f);
	EXPECT_NEAR(updated.translation.z, desiredEye.z, kLooseTolerance);
	ExpectVec3Near(updated.scale, cameraTransform.scale, kEps);

	const Vec3 aimPoint =
		targetTransform.translation + Vec3(0.0f, 0.0f, chaseCamera.lookAtHeight);
	const Vec3 expectedForward = glm::normalize(aimPoint - updated.translation);
	const Vec3 cameraForward = glm::normalize(updated.rotation * Vec3(0.0f, 1.0f, 0.0f));
	EXPECT_GT(glm::dot(cameraForward, expectedForward), 0.99f);
	EXPECT_NEAR(glm::length(updated.rotation), 1.0f, kLooseTolerance);
}
