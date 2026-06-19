#include <gtest/gtest.h>

#include "virasa/physics/PhysicsWorld.h"
#include "virasa/ecs/World.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace
{

constexpr float kEps = 1e-5f;
constexpr float kPhysicsTolerance = 0.05f;

virasa::ecs::Entity MakeEntity(uint32_t index, uint32_t generation = 1u)
{
	virasa::ecs::Entity entity;
	entity.index = index;
	entity.generation = generation;
	return entity;
}

virasa::math::Transform MakeTransform(
	const virasa::math::Vec3& translation,
	const virasa::math::Quat& rotation = virasa::math::Quat(1.0f, 0.0f, 0.0f, 0.0f),
	const virasa::math::Vec3& scale = virasa::math::Vec3(1.0f, 1.0f, 1.0f))
{
	virasa::math::Transform transform;
	transform.translation = translation;
	transform.rotation = rotation;
	transform.scale = scale;
	return transform;
}

virasa::physics::RigidBodyComponent MakeRigidBody(virasa::physics::BodyType bodyType)
{
	virasa::physics::RigidBodyComponent body;
	body.bodyType = bodyType;
	return body;
}

virasa::physics::ColliderComponent MakeBox(const virasa::math::Vec3& halfExtents)
{
	virasa::physics::ColliderComponent collider;
	collider.shape = virasa::physics::ColliderShape::Box;
	collider.halfExtents = halfExtents;
	return collider;
}

void ExpectVec3Near(
	const virasa::math::Vec3& actual,
	const virasa::math::Vec3& expected,
	float tolerance)
{
	EXPECT_NEAR(actual.x, expected.x, tolerance);
	EXPECT_NEAR(actual.y, expected.y, tolerance);
	EXPECT_NEAR(actual.z, expected.z, tolerance);
}

void ExpectQuatNear(
	const virasa::math::Quat& actual,
	const virasa::math::Quat& expected,
	float tolerance)
{
	EXPECT_NEAR(actual.w, expected.w, tolerance);
	EXPECT_NEAR(actual.x, expected.x, tolerance);
	EXPECT_NEAR(actual.y, expected.y, tolerance);
	EXPECT_NEAR(actual.z, expected.z, tolerance);
}

bool ContainsDirtyEntity(const virasa::ecs::TransformSystem& transforms, virasa::ecs::Entity entity)
{
	for (const virasa::ecs::Entity dirty : transforms.Dirty())
	{
		if (dirty == entity)
		{
			return true;
		}
	}

	return false;
}

} // namespace

TEST(PhysicsWorld, test_physics_config_describes_world_construction_parameters)
{
	virasa::physics::PhysicsConfig config;

	EXPECT_TRUE(std::is_default_constructible_v<virasa::physics::PhysicsConfig>);
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::physics::PhysicsConfig>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::physics::PhysicsConfig>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::physics::PhysicsConfig>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::physics::PhysicsConfig>);

	EXPECT_FLOAT_EQ(config.gravity.x, 0.0f);
	EXPECT_FLOAT_EQ(config.gravity.y, 0.0f);
	EXPECT_FLOAT_EQ(config.gravity.z, -9.81f);
	EXPECT_EQ(config.maxBodies, 65536u);
	EXPECT_EQ(config.maxBodyPairs, 65536u);
	EXPECT_EQ(config.maxContactConstraints, 10240u);

	virasa::physics::PhysicsConfig copy = config;
	EXPECT_FLOAT_EQ(copy.gravity.z, -9.81f);
	EXPECT_EQ(copy.maxBodies, config.maxBodies);
	EXPECT_EQ(copy.maxBodyPairs, config.maxBodyPairs);
	EXPECT_EQ(copy.maxContactConstraints, config.maxContactConstraints);
}

TEST(PhysicsWorld, test_physics_world_owns_jolt_simulation_and_body_map)
{
	EXPECT_TRUE(std::is_final_v<virasa::physics::PhysicsWorld>);
	EXPECT_FALSE(std::is_default_constructible_v<virasa::physics::PhysicsWorld>);
	EXPECT_FALSE(std::is_copy_constructible_v<virasa::physics::PhysicsWorld>);
	EXPECT_FALSE(std::is_copy_assignable_v<virasa::physics::PhysicsWorld>);
	EXPECT_FALSE(std::is_move_constructible_v<virasa::physics::PhysicsWorld>);
	EXPECT_FALSE(std::is_move_assignable_v<virasa::physics::PhysicsWorld>);

	virasa::physics::PhysicsConfig config;
	config.gravity = virasa::math::Vec3(0.0f, 0.0f, -3.0f);
	config.maxBodies = 128u;
	config.maxBodyPairs = 256u;
	config.maxContactConstraints = 64u;

	virasa::physics::PhysicsWorld first(config);
	virasa::physics::PhysicsWorld second(config);
	EXPECT_EQ(first.BodyCount(), size_t{0});
	EXPECT_EQ(second.BodyCount(), size_t{0});
	EXPECT_FALSE(first.HasBody(virasa::ecs::Entity::Invalid()));
	EXPECT_FALSE(second.HasBody(virasa::ecs::Entity::Invalid()));
}

TEST(PhysicsWorld, test_physics_world_add_body_creates_and_maps_body)
{
	virasa::physics::PhysicsWorld physics(virasa::physics::PhysicsConfig{});

	virasa::ecs::Entity boxEntity = MakeEntity(1u);
	virasa::ecs::Entity sphereEntity = MakeEntity(2u);
	virasa::ecs::Entity capsuleEntity = MakeEntity(3u);

	virasa::math::Transform boxTransform = MakeTransform(
		virasa::math::Vec3(1.0f, 2.0f, 3.0f),
		virasa::math::Quat(0.9238795f, 0.3826834f, 0.0f, 0.0f),
		virasa::math::Vec3(3.0f, 4.0f, 5.0f));

	virasa::physics::ColliderComponent box = MakeBox(virasa::math::Vec3(0.5f, 1.0f, 1.5f));
	box.offset = virasa::math::Vec3(0.1f, 0.2f, 0.3f);

	physics.AddBody(
		boxEntity,
		MakeRigidBody(virasa::physics::BodyType::Static),
		box,
		boxTransform);
	EXPECT_TRUE(physics.HasBody(boxEntity));
	EXPECT_EQ(physics.BodyCount(), size_t{1});

	virasa::physics::ColliderComponent sphere;
	sphere.shape = virasa::physics::ColliderShape::Sphere;
	sphere.radius = 0.75f;
	physics.AddBody(
		sphereEntity,
		MakeRigidBody(virasa::physics::BodyType::Dynamic),
		sphere,
		MakeTransform(virasa::math::Vec3(-2.0f, 0.0f, 4.0f)));
	EXPECT_TRUE(physics.HasBody(sphereEntity));
	EXPECT_EQ(physics.BodyCount(), size_t{2});

	virasa::physics::ColliderComponent capsule;
	capsule.shape = virasa::physics::ColliderShape::Capsule;
	capsule.radius = 0.25f;
	capsule.halfHeight = 1.0f;
	physics.AddBody(
		capsuleEntity,
		MakeRigidBody(virasa::physics::BodyType::Kinematic),
		capsule,
		MakeTransform(virasa::math::Vec3(0.0f, -3.0f, 2.0f)));
	EXPECT_TRUE(physics.HasBody(capsuleEntity));
	EXPECT_EQ(physics.BodyCount(), size_t{3});

	virasa::math::Transform storedBox = physics.GetBodyTransform(boxEntity);
	ExpectVec3Near(storedBox.translation, boxTransform.translation, kEps);
	ExpectQuatNear(storedBox.rotation, boxTransform.rotation, kEps);
	ExpectVec3Near(storedBox.scale, virasa::math::Vec3(1.0f, 1.0f, 1.0f), kEps);
}

TEST(PhysicsWorld, test_physics_world_remove_body_unmaps_and_destroys)
{
	virasa::physics::PhysicsWorld physics(virasa::physics::PhysicsConfig{});
	virasa::ecs::Entity entity = MakeEntity(7u);

	physics.AddBody(
		entity,
		MakeRigidBody(virasa::physics::BodyType::Dynamic),
		MakeBox(virasa::math::Vec3(0.5f, 0.5f, 0.5f)),
		MakeTransform(virasa::math::Vec3(0.0f, 0.0f, 2.0f)));
	ASSERT_TRUE(physics.HasBody(entity));
	ASSERT_EQ(physics.BodyCount(), size_t{1});

	physics.RemoveBody(entity);
	EXPECT_FALSE(physics.HasBody(entity));
	EXPECT_EQ(physics.BodyCount(), size_t{0});

	virasa::math::Transform removedTransform = physics.GetBodyTransform(entity);
	ExpectVec3Near(removedTransform.translation, virasa::math::Vec3(0.0f, 0.0f, 0.0f), kEps);
	ExpectQuatNear(removedTransform.rotation, virasa::math::Quat(1.0f, 0.0f, 0.0f, 0.0f), kEps);
	ExpectVec3Near(removedTransform.scale, virasa::math::Vec3(1.0f, 1.0f, 1.0f), kEps);

	physics.RemoveBody(entity);
	physics.RemoveBody(virasa::ecs::Entity::Invalid());
	EXPECT_EQ(physics.BodyCount(), size_t{0});
	EXPECT_FALSE(physics.HasBody(entity));
}

TEST(PhysicsWorld, test_physics_world_query_body_presence_and_count)
{
	virasa::physics::PhysicsWorld physics(virasa::physics::PhysicsConfig{});
	virasa::ecs::Entity entity = MakeEntity(4u, 1u);
	virasa::ecs::Entity sameIndexNewGeneration = MakeEntity(4u, 2u);
	virasa::ecs::Entity neverAdded = MakeEntity(5u, 1u);

	EXPECT_FALSE(physics.HasBody(virasa::ecs::Entity::Invalid()));
	EXPECT_FALSE(physics.HasBody(entity));
	EXPECT_FALSE(physics.HasBody(neverAdded));
	EXPECT_EQ(physics.BodyCount(), size_t{0});

	physics.AddBody(
		entity,
		MakeRigidBody(virasa::physics::BodyType::Static),
		MakeBox(virasa::math::Vec3(1.0f, 1.0f, 1.0f)),
		MakeTransform(virasa::math::Vec3(0.0f, 0.0f, 0.0f)));

	EXPECT_TRUE(physics.HasBody(entity));
	EXPECT_FALSE(physics.HasBody(sameIndexNewGeneration));
	EXPECT_FALSE(physics.HasBody(neverAdded));
	EXPECT_EQ(physics.BodyCount(), size_t{1});

	physics.RemoveBody(neverAdded);
	physics.RemoveBody(sameIndexNewGeneration);
	EXPECT_TRUE(physics.HasBody(entity));
	EXPECT_EQ(physics.BodyCount(), size_t{1});

	physics.RemoveBody(entity);
	EXPECT_FALSE(physics.HasBody(entity));
	EXPECT_EQ(physics.BodyCount(), size_t{0});
}

TEST(PhysicsWorld, test_physics_world_step_integrates_one_fixed_delta)
{
	virasa::physics::PhysicsWorld physics(virasa::physics::PhysicsConfig{});
	virasa::ecs::World world;
	virasa::ecs::Entity floorEntity = world.CreateEntity("PhysicsFloor");
	virasa::ecs::Entity boxEntity = world.CreateEntity("PhysicsBox");

	const virasa::math::Vec3 floorHalfExtents(4.0f, 4.0f, 0.1f);
	const float floorTop = 0.0f;
	const float boxHalfExtent = 0.5f;
	const virasa::math::Transform initialBoxTransform =
		MakeTransform(virasa::math::Vec3(0.0f, 0.0f, 4.0f));

	world.Transforms().Add(boxEntity, initialBoxTransform);
	world.Transforms().ClearAllDirty();

	physics.AddBody(
		floorEntity,
		MakeRigidBody(virasa::physics::BodyType::Static),
		MakeBox(floorHalfExtents),
		MakeTransform(virasa::math::Vec3(0.0f, 0.0f, floorTop - floorHalfExtents.z)));
	physics.AddBody(
		boxEntity,
		MakeRigidBody(virasa::physics::BodyType::Dynamic),
		MakeBox(virasa::math::Vec3(boxHalfExtent, boxHalfExtent, boxHalfExtent)),
		initialBoxTransform);

	const virasa::math::Transform initialBox = physics.GetBodyTransform(boxEntity);
	physics.Step(0.0f);
	physics.Step(-1.0f / 60.0f);
	ExpectVec3Near(physics.GetBodyTransform(boxEntity).translation, initialBox.translation, kEps);

	const float fixedDelta = 1.0f / 60.0f;
	for (int i = 0; i < 240; ++i)
	{
		physics.Step(fixedDelta);
	}

	const virasa::math::Transform settledBox = physics.GetBodyTransform(boxEntity);
	EXPECT_LT(settledBox.translation.z, initialBox.translation.z);
	EXPECT_NEAR(settledBox.translation.z, floorTop + boxHalfExtent, kPhysicsTolerance);

	for (int i = 0; i < 60; ++i)
	{
		physics.Step(fixedDelta);
	}

	const virasa::math::Transform finalBox = physics.GetBodyTransform(boxEntity);
	EXPECT_NEAR(finalBox.translation.z, settledBox.translation.z, kPhysicsTolerance);
	EXPECT_NEAR(finalBox.translation.z, floorTop + boxHalfExtent, kPhysicsTolerance);

	const virasa::math::Transform floorTransform = physics.GetBodyTransform(floorEntity);
	EXPECT_NEAR(floorTransform.translation.z, floorTop - floorHalfExtents.z, kEps);

	physics.SyncToWorld(world);
	const virasa::math::Transform& synced = world.Transforms().GetLocal(boxEntity);
	EXPECT_NEAR(synced.translation.z, finalBox.translation.z, kPhysicsTolerance);
	EXPECT_TRUE(ContainsDirtyEntity(world.Transforms(), boxEntity));
}

TEST(PhysicsWorld, test_physics_world_set_and_get_body_transform)
{
	virasa::physics::PhysicsWorld physics(virasa::physics::PhysicsConfig{});
	virasa::ecs::Entity dynamicEntity = MakeEntity(20u);
	virasa::ecs::Entity staticEntity = MakeEntity(21u);
	virasa::ecs::Entity kinematicEntity = MakeEntity(22u);
	virasa::ecs::Entity missingEntity = MakeEntity(23u);

	physics.AddBody(
		dynamicEntity,
		MakeRigidBody(virasa::physics::BodyType::Dynamic),
		MakeBox(virasa::math::Vec3(0.5f, 0.5f, 0.5f)),
		MakeTransform(virasa::math::Vec3(0.0f, 0.0f, 1.0f)));
	physics.AddBody(
		staticEntity,
		MakeRigidBody(virasa::physics::BodyType::Static),
		MakeBox(virasa::math::Vec3(1.0f, 1.0f, 1.0f)),
		MakeTransform(virasa::math::Vec3(0.0f, 0.0f, -1.0f)));
	physics.AddBody(
		kinematicEntity,
		MakeRigidBody(virasa::physics::BodyType::Kinematic),
		MakeBox(virasa::math::Vec3(0.5f, 0.5f, 0.5f)),
		MakeTransform(virasa::math::Vec3(0.0f, 0.0f, 2.0f)));

	const virasa::math::Transform target = MakeTransform(
		virasa::math::Vec3(5.0f, -2.0f, 3.0f),
		virasa::math::Quat(0.9238795f, 0.0f, 0.0f, 0.3826834f),
		virasa::math::Vec3(7.0f, 8.0f, 9.0f));

	physics.SetBodyTransform(dynamicEntity, target);
	physics.SetBodyTransform(staticEntity, target);
	physics.SetBodyTransform(kinematicEntity, target);
	physics.SetBodyTransform(missingEntity, target);

	for (virasa::ecs::Entity entity : {dynamicEntity, staticEntity, kinematicEntity})
	{
		virasa::math::Transform actual = physics.GetBodyTransform(entity);
		ExpectVec3Near(actual.translation, target.translation, kEps);
		ExpectQuatNear(actual.rotation, target.rotation, kEps);
		ExpectVec3Near(actual.scale, virasa::math::Vec3(1.0f, 1.0f, 1.0f), kEps);

		const float rotationLength = std::sqrt(
			actual.rotation.w * actual.rotation.w +
			actual.rotation.x * actual.rotation.x +
			actual.rotation.y * actual.rotation.y +
			actual.rotation.z * actual.rotation.z);
		EXPECT_NEAR(rotationLength, 1.0f, kEps);
	}

	virasa::math::Transform missing = physics.GetBodyTransform(missingEntity);
	ExpectVec3Near(missing.translation, virasa::math::Vec3(0.0f, 0.0f, 0.0f), kEps);
	ExpectQuatNear(missing.rotation, virasa::math::Quat(1.0f, 0.0f, 0.0f, 0.0f), kEps);
	ExpectVec3Near(missing.scale, virasa::math::Vec3(1.0f, 1.0f, 1.0f), kEps);
}

TEST(PhysicsWorld, test_physics_world_sync_to_world_writes_dynamic_transforms)
{
	virasa::physics::PhysicsWorld physics(virasa::physics::PhysicsConfig{});
	virasa::ecs::World world;

	virasa::ecs::Entity syncedEntity = world.CreateEntity("Synced");
	virasa::ecs::Entity skippedEntity = world.CreateEntity("Skipped");

	const virasa::math::Vec3 preservedScale(10.0f, 10.0f, 0.2f);
	virasa::math::Transform initial = MakeTransform(
		virasa::math::Vec3(0.0f, 0.0f, 0.0f),
		virasa::math::Quat(1.0f, 0.0f, 0.0f, 0.0f),
		preservedScale);
	world.Transforms().Add(syncedEntity, initial);
	world.Transforms().ClearAllDirty();

	physics.AddBody(
		syncedEntity,
		MakeRigidBody(virasa::physics::BodyType::Dynamic),
		MakeBox(virasa::math::Vec3(0.5f, 0.5f, 0.5f)),
		initial);
	physics.AddBody(
		skippedEntity,
		MakeRigidBody(virasa::physics::BodyType::Static),
		MakeBox(virasa::math::Vec3(1.0f, 1.0f, 1.0f)),
		MakeTransform(virasa::math::Vec3(10.0f, 10.0f, 10.0f)));

	const virasa::math::Transform simulated = MakeTransform(
		virasa::math::Vec3(2.0f, 3.0f, 4.0f),
		virasa::math::Quat(0.9238795f, 0.0f, 0.3826834f, 0.0f),
		virasa::math::Vec3(2.0f, 2.0f, 2.0f));
	physics.SetBodyTransform(syncedEntity, simulated);

	physics.SyncToWorld(world);

	ASSERT_TRUE(world.Transforms().Has(syncedEntity));
	const virasa::math::Transform& local = world.Transforms().GetLocal(syncedEntity);
	ExpectVec3Near(local.translation, simulated.translation, kEps);
	ExpectQuatNear(local.rotation, simulated.rotation, kEps);
	ExpectVec3Near(local.scale, preservedScale, kEps);
	EXPECT_TRUE(ContainsDirtyEntity(world.Transforms(), syncedEntity));
	EXPECT_FALSE(world.Transforms().Has(skippedEntity));
	EXPECT_EQ(world.GetEntityCount(), 2u);
}
