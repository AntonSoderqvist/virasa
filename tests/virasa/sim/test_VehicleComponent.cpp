#include <gtest/gtest.h>

#include "virasa/sim/VehicleComponent.h"

#include <type_traits>

using namespace virasa::ecs;
using namespace virasa::input;
using namespace virasa::sim;

namespace
{

void ExpectInvalidWheels(const VehicleWheels& wheels)
{
	EXPECT_EQ(wheels.frontRight, Entity::Invalid());
	EXPECT_EQ(wheels.frontLeft, Entity::Invalid());
	EXPECT_EQ(wheels.backRight, Entity::Invalid());
	EXPECT_EQ(wheels.backLeft, Entity::Invalid());
}

} // namespace

TEST(VehicleComponent, test_vehicle_wheels_names_four_wheel_entities)
{
	static_assert(std::is_copy_constructible_v<VehicleWheels>);
	static_assert(std::is_copy_assignable_v<VehicleWheels>);
	static_assert(std::is_move_constructible_v<VehicleWheels>);
	static_assert(std::is_move_assignable_v<VehicleWheels>);
	static_assert(std::is_default_constructible_v<VehicleWheels>);
	static_assert(std::is_trivially_destructible_v<VehicleWheels>);
	static_assert(std::is_trivially_copyable_v<VehicleWheels>);

	VehicleWheels wheels;
	ExpectInvalidWheels(wheels);

	wheels.frontRight = Entity{1u, 10u};
	wheels.frontLeft = Entity{2u, 20u};
	wheels.backRight = Entity{3u, 30u};
	wheels.backLeft = Entity{4u, 40u};

	EXPECT_EQ(wheels.frontRight, (Entity{1u, 10u}));
	EXPECT_EQ(wheels.frontLeft, (Entity{2u, 20u}));
	EXPECT_EQ(wheels.backRight, (Entity{3u, 30u}));
	EXPECT_EQ(wheels.backLeft, (Entity{4u, 40u}));
}

TEST(VehicleComponent, test_vehicle_component_holds_authored_vehicle_parameters)
{
	static_assert(std::is_copy_constructible_v<VehicleComponent>);
	static_assert(std::is_copy_assignable_v<VehicleComponent>);
	static_assert(std::is_move_constructible_v<VehicleComponent>);
	static_assert(std::is_move_assignable_v<VehicleComponent>);
	static_assert(std::is_default_constructible_v<VehicleComponent>);
	static_assert(std::is_trivially_destructible_v<VehicleComponent>);
	static_assert(std::is_trivially_copyable_v<VehicleComponent>);

	VehicleComponent vehicle;
	ExpectInvalidWheels(vehicle.wheels);
	EXPECT_FLOAT_EQ(vehicle.suspensionRestLength, 0.6f);
	EXPECT_FLOAT_EQ(vehicle.suspensionStiffness, 30000.0f);
	EXPECT_FLOAT_EQ(vehicle.suspensionDamping, 4000.0f);
	EXPECT_FLOAT_EQ(vehicle.maxSteerAngle, 0.5235987755982988f);
	EXPECT_FLOAT_EQ(vehicle.enginePower, 15000.0f);
	EXPECT_FLOAT_EQ(vehicle.brakeForce, 10000.0f);
	EXPECT_FLOAT_EQ(vehicle.tireGrip, 8000.0f);
	EXPECT_EQ(vehicle.throttleAction, static_cast<ActionId>(0));
	EXPECT_EQ(vehicle.steerAction, static_cast<ActionId>(1));
	EXPECT_EQ(vehicle.brakeAction, static_cast<ActionId>(2));
	EXPECT_EQ(vehicle.handbrakeAction, static_cast<ActionId>(3));

	vehicle.wheels.frontRight = Entity{11u, 1u};
	vehicle.wheels.frontLeft = Entity{12u, 1u};
	vehicle.wheels.backRight = Entity{13u, 1u};
	vehicle.wheels.backLeft = Entity{14u, 1u};

	EXPECT_EQ(vehicle.wheels.frontRight, (Entity{11u, 1u}));
	EXPECT_EQ(vehicle.wheels.frontLeft, (Entity{12u, 1u}));
	EXPECT_EQ(vehicle.wheels.backRight, (Entity{13u, 1u}));
	EXPECT_EQ(vehicle.wheels.backLeft, (Entity{14u, 1u}));
}
