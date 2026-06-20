#include <gtest/gtest.h>

#include "virasa/sim/behaviors/VehicleBehavior.h"

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/TransformSystem.h"
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

#include <cmath>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>

using namespace virasa::ecs;
using namespace virasa::input;
using namespace virasa::math;
using namespace virasa::physics;
using namespace virasa::sim;
using namespace virasa::sim::behaviors;

namespace
{

constexpr float kEps = 1e-5f;
constexpr float kPhysicsTolerance = 0.05f;

void RegisterPhysicsSystems(World& world)
{
	(void)world.RegisterSystem(std::make_unique<SparseComponentSystem>(
		kInvalidComponentId,
		"RigidBody",
		sizeof(RigidBodyComponent)));
	(void)world.RegisterSystem(std::make_unique<SparseComponentSystem>(
		kInvalidComponentId,
		"Collider",
		sizeof(ColliderComponent)));
}

void RegisterVehicleSystem(World& world)
{
	(void)world.RegisterSystem(std::make_unique<SparseComponentSystem>(
		kInvalidComponentId,
		"Vehicle",
		sizeof(VehicleComponent)));
}

void RegisterWheelSystem(World& world)
{
	(void)world.RegisterSystem(std::make_unique<SparseComponentSystem>(
		kInvalidComponentId,
		"Wheel",
		sizeof(WheelComponent)));
}

Transform MakeTransform(const Vec3& translation)
{
	Transform transform;
	transform.translation = translation;
	return transform;
}

Transform MakeWheelTransform(const Vec3& translation)
{
	Transform transform;
	transform.translation = translation;
	transform.scale = Vec3(0.8f, 0.25f, 0.8f);
	return transform;
}

RigidBodyComponent MakeRigidBody(BodyType bodyType)
{
	RigidBodyComponent body;
	body.bodyType = bodyType;
	return body;
}

ColliderComponent MakeBox(const Vec3& halfExtents)
{
	ColliderComponent collider;
	collider.shape = ColliderShape::Box;
	collider.halfExtents = halfExtents;
	return collider;
}

VehicleComponent MakeVehicle()
{
	VehicleComponent vehicle;
	vehicle.suspensionRestLength = 0.8f;
	vehicle.suspensionStiffness = 30000.0f;
	vehicle.suspensionDamping = 1000.0f;
	vehicle.enginePower = 24000.0f;
	vehicle.tireGrip = 6000.0f;
	return vehicle;
}

void AddPhysicsComponents(
	World& world,
	Entity entity,
	const RigidBodyComponent& body,
	const ColliderComponent& collider)
{
	const ComponentId rigidBodyId = world.GetSystemId("RigidBody");
	const ComponentId colliderId = world.GetSystemId("Collider");
	ASSERT_NE(rigidBodyId, kInvalidComponentId);
	ASSERT_NE(colliderId, kInvalidComponentId);

	world.GetSystem(rigidBodyId).AddRaw(entity, &body);
	world.GetSystem(colliderId).AddRaw(entity, &collider);
}

void AddVehicleComponent(World& world, Entity entity, const VehicleComponent& vehicle)
{
	const ComponentId vehicleId = world.GetSystemId("Vehicle");
	ASSERT_NE(vehicleId, kInvalidComponentId);

	world.GetSystem(vehicleId).AddRaw(entity, &vehicle);
}

void AddWheelComponent(World& world, Entity entity, const WheelComponent& wheel)
{
	const ComponentId wheelId = world.GetSystemId("Wheel");
	ASSERT_NE(wheelId, kInvalidComponentId);

	world.GetSystem(wheelId).AddRaw(entity, &wheel);
}

Entity CreateWheel(
	World& world,
	Entity chassis,
	const char* name,
	const Vec3& mount,
	bool steered,
	bool driven)
{
	Entity wheelEntity = world.CreateEntity(name);
	EXPECT_EQ(world.SetParent(wheelEntity, chassis), EcsError::None);
	world.Transforms().Add(wheelEntity, MakeWheelTransform(mount));

	WheelComponent wheel;
	wheel.steered = steered;
	wheel.driven = driven;
	wheel.radiusFactor = 1.0f;
	AddWheelComponent(world, wheelEntity, wheel);
	return wheelEntity;
}

VehicleWheels CreateVehicleWheels(World& world, Entity chassis)
{
	VehicleWheels wheels;
	wheels.frontRight = CreateWheel(
		world,
		chassis,
		"FrontRightWheel",
		Vec3(0.45f, 0.75f, 0.0f),
		true,
		false);
	wheels.frontLeft = CreateWheel(
		world,
		chassis,
		"FrontLeftWheel",
		Vec3(-0.45f, 0.75f, 0.0f),
		true,
		false);
	wheels.backRight = CreateWheel(
		world,
		chassis,
		"BackRightWheel",
		Vec3(0.45f, -0.75f, 0.0f),
		false,
		true);
	wheels.backLeft = CreateWheel(
		world,
		chassis,
		"BackLeftWheel",
		Vec3(-0.45f, -0.75f, 0.0f),
		false,
		true);
	return wheels;
}

void ExpectVec3Near(const Vec3& actual, const Vec3& expected, float tolerance)
{
	EXPECT_NEAR(actual.x, expected.x, tolerance);
	EXPECT_NEAR(actual.y, expected.y, tolerance);
	EXPECT_NEAR(actual.z, expected.z, tolerance);
}

} // namespace

TEST(VehicleBehavior, test_vehicle_behavior_is_concrete_vehicle_system)
{
	static_assert(std::is_final_v<VehicleBehavior>);
	static_assert(std::is_base_of_v<Behavior, VehicleBehavior>);
	static_assert(std::is_default_constructible_v<VehicleBehavior>);
	static_assert(!std::is_copy_constructible_v<VehicleBehavior>);
	static_assert(!std::is_copy_assignable_v<VehicleBehavior>);
	static_assert(!std::is_move_constructible_v<VehicleBehavior>);
	static_assert(!std::is_move_assignable_v<VehicleBehavior>);
	static_assert(std::has_virtual_destructor_v<VehicleBehavior>);

	std::unique_ptr<Behavior> behavior = std::make_unique<VehicleBehavior>();
	EXPECT_EQ(std::string(behavior->Name()), "Vehicle");
	EXPECT_STREQ(behavior->Name(), "Vehicle");
}

TEST(VehicleBehavior, test_vehicle_behavior_resolves_shared_world_and_inputs)
{
	VehicleBehavior behavior;
	TickContext tick;
	tick.deltaTime = 1.0f / 60.0f;

	World noPhysicsWorld;
	RegisterPhysicsSystems(noPhysicsWorld);
	RegisterVehicleSystem(noPhysicsWorld);
	RegisterWheelSystem(noPhysicsWorld);

	Entity noPhysicsVehicle = noPhysicsWorld.CreateEntity("NoPhysicsVehicle");
	const Transform noPhysicsTransform = MakeTransform(Vec3(0.0f, 0.0f, 0.8f));
	noPhysicsWorld.Transforms().Add(noPhysicsVehicle, noPhysicsTransform);
	AddPhysicsComponents(
		noPhysicsWorld,
		noPhysicsVehicle,
		MakeRigidBody(BodyType::Dynamic),
		MakeBox(Vec3(0.6f, 1.0f, 0.2f)));
	VehicleComponent noPhysicsVehicleComponent = MakeVehicle();
	noPhysicsVehicleComponent.wheels = CreateVehicleWheels(noPhysicsWorld, noPhysicsVehicle);
	AddVehicleComponent(noPhysicsWorld, noPhysicsVehicle, noPhysicsVehicleComponent);

	const uint32_t noPhysicsEntityCount = noPhysicsWorld.GetEntityCount();
	const size_t noPhysicsVehicleCount =
		noPhysicsWorld.GetSystem(noPhysicsWorld.GetSystemId("Vehicle")).Size();
	const Transform noPhysicsWheelTransform =
		noPhysicsWorld.Transforms().GetLocal(noPhysicsVehicleComponent.wheels.frontRight);

	CommandBuffer missingPhysicsCommands;
	behavior.Step(noPhysicsWorld, tick, missingPhysicsCommands);

	EXPECT_TRUE(missingPhysicsCommands.IsEmpty());
	EXPECT_EQ(noPhysicsWorld.GetEntityCount(), noPhysicsEntityCount);
	EXPECT_EQ(noPhysicsWorld.GetSystem(noPhysicsWorld.GetSystemId("Vehicle")).Size(),
		noPhysicsVehicleCount);
	ExpectVec3Near(
		noPhysicsWorld.Transforms().GetLocal(noPhysicsVehicle).translation,
		noPhysicsTransform.translation,
		kEps);
	ExpectVec3Near(
		noPhysicsWorld.Transforms().GetLocal(
			noPhysicsVehicleComponent.wheels.frontRight).translation,
		noPhysicsWheelTransform.translation,
		kEps);

	World noVehicleSystemWorld;
	RegisterPhysicsSystems(noVehicleSystemWorld);

	PhysicsConfig config;
	config.gravity = Vec3(0.0f, 0.0f, 0.0f);
	PhysicsWorld physics(config);
	noVehicleSystemWorld.SetResource(std::type_index(typeid(PhysicsWorld)), &physics);

	Entity chassis = noVehicleSystemWorld.CreateEntity("NoVehicleSystemChassis");
	const Transform chassisTransform = MakeTransform(Vec3(0.0f, 0.0f, 1.0f));
	noVehicleSystemWorld.Transforms().Add(chassis, chassisTransform);
	const RigidBodyComponent chassisBody = MakeRigidBody(BodyType::Dynamic);
	const ColliderComponent chassisCollider = MakeBox(Vec3(0.6f, 1.0f, 0.2f));
	AddPhysicsComponents(noVehicleSystemWorld, chassis, chassisBody, chassisCollider);
	physics.AddBody(chassis, chassisBody, chassisCollider, chassisTransform);

	CommandBuffer missingVehicleCommands;
	behavior.Step(noVehicleSystemWorld, tick, missingVehicleCommands);
	physics.Step(tick.deltaTime);

	EXPECT_TRUE(missingVehicleCommands.IsEmpty());
	EXPECT_EQ(noVehicleSystemWorld.GetSystemId("Vehicle"), kInvalidComponentId);
	ExpectVec3Near(physics.GetLinearVelocity(chassis), Vec3(0.0f, 0.0f, 0.0f),
		kPhysicsTolerance);

	World noWheelSystemWorld;
	RegisterPhysicsSystems(noWheelSystemWorld);
	RegisterVehicleSystem(noWheelSystemWorld);

	PhysicsConfig noWheelConfig;
	noWheelConfig.gravity = Vec3(0.0f, 0.0f, 0.0f);
	PhysicsWorld noWheelPhysics(noWheelConfig);
	noWheelSystemWorld.SetResource(std::type_index(typeid(PhysicsWorld)), &noWheelPhysics);

	Entity noWheelFloor = noWheelSystemWorld.CreateEntity("NoWheelFloor");
	const Transform noWheelFloorTransform = MakeTransform(Vec3(0.0f, 0.0f, -0.1f));
	noWheelSystemWorld.Transforms().Add(noWheelFloor, noWheelFloorTransform);
	RigidBodyComponent noWheelFloorBody = MakeRigidBody(BodyType::Static);
	const ColliderComponent noWheelFloorCollider = MakeBox(Vec3(8.0f, 8.0f, 0.1f));
	AddPhysicsComponents(
		noWheelSystemWorld,
		noWheelFloor,
		noWheelFloorBody,
		noWheelFloorCollider);
	noWheelPhysics.AddBody(
		noWheelFloor,
		noWheelFloorBody,
		noWheelFloorCollider,
		noWheelFloorTransform);

	Entity noWheelChassis = noWheelSystemWorld.CreateEntity("NoWheelSystemChassis");
	const Transform noWheelChassisTransform = MakeTransform(Vec3(0.0f, 0.0f, 0.8f));
	noWheelSystemWorld.Transforms().Add(noWheelChassis, noWheelChassisTransform);
	RigidBodyComponent noWheelChassisBody = MakeRigidBody(BodyType::Dynamic);
	noWheelChassisBody.linearDamping = 0.0f;
	const ColliderComponent noWheelChassisCollider = MakeBox(Vec3(0.6f, 1.0f, 0.2f));
	AddPhysicsComponents(
		noWheelSystemWorld,
		noWheelChassis,
		noWheelChassisBody,
		noWheelChassisCollider);
	AddVehicleComponent(noWheelSystemWorld, noWheelChassis, MakeVehicle());
	noWheelPhysics.AddBody(
		noWheelChassis,
		noWheelChassisBody,
		noWheelChassisCollider,
		noWheelChassisTransform);

	ActionState noWheelFrameActions;
	noWheelFrameActions.SetAxis(static_cast<ActionId>(0), 1.0f);
	StepBridge noWheelBridge;
	noWheelBridge.LatchFrame(noWheelFrameActions);
	noWheelBridge.Publish(noWheelSystemWorld, true);

	CommandBuffer missingWheelCommands;
	behavior.Step(noWheelSystemWorld, tick, missingWheelCommands);
	noWheelPhysics.Step(tick.deltaTime);

	EXPECT_TRUE(missingWheelCommands.IsEmpty());
	EXPECT_EQ(noWheelSystemWorld.GetSystemId("Wheel"), kInvalidComponentId);
	ExpectVec3Near(
		noWheelPhysics.GetLinearVelocity(noWheelChassis),
		Vec3(0.0f, 0.0f, 0.0f),
		kPhysicsTolerance);
}

TEST(VehicleBehavior, test_vehicle_behavior_applies_wheel_forces_per_step)
{
	VehicleBehavior behavior;
	TickContext tick;
	tick.deltaTime = 1.0f / 60.0f;

	World world;
	RegisterPhysicsSystems(world);
	RegisterVehicleSystem(world);
	RegisterWheelSystem(world);

	Entity floor = world.CreateEntity("VehicleFloor");
	const Transform floorTransform = MakeTransform(Vec3(0.0f, 0.0f, -0.1f));
	world.Transforms().Add(floor, floorTransform);
	RigidBodyComponent floorBody = MakeRigidBody(BodyType::Static);
	floorBody.friction = 1.0f;
	const ColliderComponent floorCollider = MakeBox(Vec3(8.0f, 8.0f, 0.1f));
	AddPhysicsComponents(world, floor, floorBody, floorCollider);

	Entity chassis = world.CreateEntity("VehicleChassis");
	const Transform chassisTransform = MakeTransform(Vec3(0.0f, 0.0f, 0.8f));
	world.Transforms().Add(chassis, chassisTransform);
	RigidBodyComponent chassisBody = MakeRigidBody(BodyType::Dynamic);
	chassisBody.mass = 1000.0f;
	chassisBody.linearDamping = 0.0f;
	chassisBody.angularDamping = 0.0f;
	const ColliderComponent chassisCollider = MakeBox(Vec3(0.6f, 1.0f, 0.2f));
	AddPhysicsComponents(world, chassis, chassisBody, chassisCollider);
	VehicleComponent vehicle = MakeVehicle();
	vehicle.wheels = CreateVehicleWheels(world, chassis);
	AddVehicleComponent(world, chassis, vehicle);

	PhysicsWorld physics(PhysicsConfig{});
	physics.AddBody(floor, floorBody, floorCollider, floorTransform);
	physics.AddBody(chassis, chassisBody, chassisCollider, chassisTransform);
	world.SetResource(std::type_index(typeid(PhysicsWorld)), &physics);

	ActionState frameActions;
	frameActions.SetAxis(static_cast<ActionId>(0), 1.0f);
	StepBridge bridge;
	bridge.LatchFrame(frameActions);
	bridge.Publish(world, true);

	const uint32_t entityCountBefore = world.GetEntityCount();
	const size_t vehicleCountBefore = world.GetSystem(world.GetSystemId("Vehicle")).Size();
	const size_t rigidBodyCountBefore = world.GetSystem(world.GetSystemId("RigidBody")).Size();
	const Vec3 initialVelocity = physics.GetLinearVelocity(chassis);

	CommandBuffer commands;
	behavior.Step(world, tick, commands);

	EXPECT_TRUE(commands.IsEmpty());
	EXPECT_EQ(world.GetEntityCount(), entityCountBefore);
	EXPECT_EQ(world.GetSystem(world.GetSystemId("Vehicle")).Size(), vehicleCountBefore);
	EXPECT_EQ(world.GetSystem(world.GetSystemId("RigidBody")).Size(), rigidBodyCountBefore);
	ExpectVec3Near(physics.GetBodyTransform(chassis).translation,
		chassisTransform.translation,
		kEps);

	physics.Step(tick.deltaTime);

	const Vec3 velocityAfterVehicleForces = physics.GetLinearVelocity(chassis);
	EXPECT_GT(velocityAfterVehicleForces.z, initialVelocity.z + 0.1f);
	EXPECT_GT(velocityAfterVehicleForces.y, initialVelocity.y + 0.05f);
	EXPECT_GT(physics.GetBodyTransform(chassis).translation.z, chassisTransform.translation.z);
}

TEST(VehicleBehavior, test_vehicle_behavior_writes_wheel_visual_pose)
{
	VehicleBehavior behavior;
	TickContext tick;
	tick.deltaTime = 1.0f / 60.0f;

	World world;
	RegisterPhysicsSystems(world);
	RegisterVehicleSystem(world);
	RegisterWheelSystem(world);

	Entity floor = world.CreateEntity("VehicleFloor");
	const Transform floorTransform = MakeTransform(Vec3(0.0f, 0.0f, -0.1f));
	world.Transforms().Add(floor, floorTransform);
	RigidBodyComponent floorBody = MakeRigidBody(BodyType::Static);
	floorBody.friction = 1.0f;
	const ColliderComponent floorCollider = MakeBox(Vec3(8.0f, 8.0f, 0.1f));
	AddPhysicsComponents(world, floor, floorBody, floorCollider);

	Entity chassis = world.CreateEntity("VehicleChassis");
	const Transform chassisTransform = MakeTransform(Vec3(0.0f, 0.0f, 0.8f));
	world.Transforms().Add(chassis, chassisTransform);
	RigidBodyComponent chassisBody = MakeRigidBody(BodyType::Dynamic);
	chassisBody.mass = 1000.0f;
	chassisBody.linearDamping = 0.0f;
	chassisBody.angularDamping = 0.0f;
	const ColliderComponent chassisCollider = MakeBox(Vec3(0.6f, 1.0f, 0.2f));
	AddPhysicsComponents(world, chassis, chassisBody, chassisCollider);

	VehicleComponent vehicle = MakeVehicle();
	vehicle.wheels = CreateVehicleWheels(world, chassis);
	AddVehicleComponent(world, chassis, vehicle);

	const Transform authoredFrontRight =
		world.Transforms().GetLocal(vehicle.wheels.frontRight);
	const Transform authoredBackRight =
		world.Transforms().GetLocal(vehicle.wheels.backRight);

	PhysicsConfig config;
	config.gravity = Vec3(0.0f, 0.0f, 0.0f);
	PhysicsWorld physics(config);
	physics.AddBody(floor, floorBody, floorCollider, floorTransform);
	physics.AddBody(chassis, chassisBody, chassisCollider, chassisTransform);
	world.SetResource(std::type_index(typeid(PhysicsWorld)), &physics);

	ActionState frameActions;
	frameActions.SetAxis(static_cast<ActionId>(1), 1.0f);
	StepBridge bridge;
	bridge.LatchFrame(frameActions);
	bridge.Publish(world, true);

	CommandBuffer commands;
	behavior.Step(world, tick, commands);

	EXPECT_TRUE(commands.IsEmpty());

	const Transform frontRight = world.Transforms().GetLocal(vehicle.wheels.frontRight);
	const Transform backRight = world.Transforms().GetLocal(vehicle.wheels.backRight);

	ExpectVec3Near(frontRight.scale, authoredFrontRight.scale, kEps);
	ExpectVec3Near(backRight.scale, authoredBackRight.scale, kEps);
	EXPECT_NEAR(frontRight.translation.x, authoredFrontRight.translation.x, kEps);
	EXPECT_NEAR(frontRight.translation.y, authoredFrontRight.translation.y, kEps);
	EXPECT_LT(frontRight.translation.z, authoredFrontRight.translation.z);
	EXPECT_GT(frontRight.translation.z, authoredFrontRight.translation.z -
		vehicle.suspensionRestLength - kEps);

	EXPECT_GT(std::abs(frontRight.rotation.z), 0.01f);
	EXPECT_NEAR(backRight.rotation.z, 0.0f, kEps);
	EXPECT_NEAR(backRight.rotation.w, 1.0f, kEps);
	EXPECT_NEAR(backRight.translation.z, authoredBackRight.translation.z -
		0.5f * vehicle.suspensionRestLength,
		0.05f);
}
