#include <gtest/gtest.h>

#include "virasa/ecs/Components.h"
#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/World.h"
#include "virasa/math/Transform.h"
#include "virasa/sim/BehaviorRegistry.h"
#include "virasa/sim/Scene.h"
#include "virasa/sim/Tick.h"

#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

using namespace virasa::ecs;
using namespace virasa::math;
using namespace virasa::sim;

namespace
{

class TestBehavior final : public Behavior
{
public:
	TestBehavior(const char* name, std::vector<std::string>* log)
		: _name(name)
		, _log(log)
	{
	}

	const char* Name() const noexcept override
	{
		return _name.c_str();
	}

	void Step(World&, const TickContext&, CommandBuffer&) override
	{
		if (_log != nullptr)
		{
			_log->push_back(_name);
		}
	}

private:
	std::string _name;
	std::vector<std::string>* _log = nullptr;
};

} // namespace

TEST(Scene, test_scene_owns_world_behavior_manifest_and_metadata)
{
	static_assert(!std::is_copy_constructible_v<Scene>);
	static_assert(!std::is_copy_assignable_v<Scene>);
	static_assert(std::is_move_constructible_v<Scene>);
	static_assert(std::is_move_assignable_v<Scene>);
	static_assert(std::is_default_constructible_v<Scene>);

	Scene scene;
	EXPECT_EQ(scene.GetWorld().GetEntityCount(), 0u);
	EXPECT_NE(scene.GetWorld().GetSystemId("Transform"), kInvalidComponentId);
	EXPECT_TRUE(scene.Behaviors().empty());
	EXPECT_TRUE(scene.GetName().empty());
	EXPECT_EQ(scene.GetDefaultCamera(), Entity::Invalid());

	Entity entity = scene.GetWorld().CreateEntity("Owned");
	EXPECT_TRUE(scene.GetWorld().IsValid(entity));
}

TEST(Scene, test_scene_metadata_name_and_default_camera)
{
	Scene scene;
	Entity camera = scene.GetWorld().CreateEntity("Camera");

	scene.SetName("Level01");
	scene.SetDefaultCamera(camera);

	EXPECT_EQ(scene.GetName(), "Level01");
	EXPECT_EQ(scene.GetDefaultCamera(), camera);

	scene.SetName("Renamed");
	scene.SetDefaultCamera(Entity::Invalid());

	EXPECT_EQ(scene.GetName(), "Renamed");
	EXPECT_EQ(scene.GetDefaultCamera(), Entity::Invalid());
}

TEST(Scene, test_scene_behavior_manifest_references_behaviors_by_name)
{
	Scene scene;
	scene.AddBehavior("Player");
	scene.AddBehavior("Enemy");
	scene.AddBehavior("Player");

	EXPECT_TRUE(scene.HasBehavior("Player"));
	EXPECT_TRUE(scene.HasBehavior("Enemy"));
	EXPECT_FALSE(scene.HasBehavior("player"));

	const std::vector<std::string> expected = {"Player", "Enemy", "Player"};
	EXPECT_EQ(scene.Behaviors(), expected);
}

TEST(Scene, test_scene_instantiate_produces_independent_duplicate)
{
	Scene scene;
	scene.SetName("Authored");
	scene.AddBehavior("Player");

	Entity camera = scene.GetWorld().CreateEntity("Camera");
	Entity meshEntity = scene.GetWorld().CreateEntity("Mesh");
	scene.SetDefaultCamera(camera);

	Transform cameraTransform;
	cameraTransform.translation = Vec3(1.0f, 2.0f, 3.0f);
	scene.GetWorld().Transforms().Add(camera, cameraTransform);
	scene.GetWorld().UpdateTransforms();

	ComponentId meshId = scene.GetWorld().GetSystemId("Mesh");
	ASSERT_NE(meshId, kInvalidComponentId);
	MeshComponent mesh;
	mesh.meshId = 77u;
	scene.GetWorld().GetSystem(meshId).AddRaw(meshEntity, &mesh);

	Scene instance = scene.Instantiate();

	EXPECT_EQ(instance.GetName(), "Authored");
	EXPECT_EQ(instance.GetDefaultCamera(), camera);
	EXPECT_EQ(instance.Behaviors(), scene.Behaviors());
	EXPECT_TRUE(instance.GetWorld().IsValid(camera));
	EXPECT_TRUE(instance.GetWorld().IsValid(meshEntity));
	EXPECT_NE(&instance.GetWorld().Transforms(), &scene.GetWorld().Transforms());
	EXPECT_NEAR(instance.GetWorld().Transforms().GetWorld(camera)[3][0], 1.0f, 1e-5f);

	MeshComponent clonedMesh;
	std::memcpy(&clonedMesh, instance.GetWorld().GetSystem(meshId).GetRaw(meshEntity), sizeof(MeshComponent));
	EXPECT_EQ(clonedMesh.meshId, 77u);

	scene.SetName("SourceMutated");
	scene.GetWorld().SetName(camera, "SourceCamera");
	scene.GetWorld().GetSystem(meshId).Remove(meshEntity);

	EXPECT_EQ(instance.GetName(), "Authored");
	EXPECT_EQ(instance.GetWorld().GetNameComponent(camera).name, "Camera");
	EXPECT_TRUE(instance.GetWorld().GetSystem(meshId).Has(meshEntity));

	instance.SetName("InstanceMutated");
	instance.GetWorld().SetName(meshEntity, "InstanceMesh");
	EXPECT_EQ(scene.GetName(), "SourceMutated");
	EXPECT_EQ(scene.GetWorld().GetNameComponent(meshEntity).name, "Mesh");
}

TEST(Scene, test_scene_build_scheduler_populates_from_manifest)
{
	Scene scene;
	scene.AddBehavior("First");
	scene.AddBehavior("Missing");
	scene.AddBehavior("Second");

	std::vector<std::string> log;
	BehaviorRegistry registry;
	registry.Register("First", Phase::Step, [&]()
	{
		return std::make_unique<TestBehavior>("First", &log);
	});
	registry.Register("Second", Phase::Step, [&]()
	{
		return std::make_unique<TestBehavior>("Second", &log);
	});

	Scheduler scheduler;
	EXPECT_FALSE(scene.BuildScheduler(registry, scheduler));
	EXPECT_EQ(scheduler.BehaviorCount(Phase::Step), 2u);

	World world;
	TickContext tick;
	scheduler.Step(world, tick);

	const std::vector<std::string> expected = {"First", "Second"};
	EXPECT_EQ(log, expected);
}
