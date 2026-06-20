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
	Entity coreChild = scene.GetWorld().CreateEntity("CoreChild");
	Entity editorRoot = scene.GetWorld().CreateEntity("EditorRoot");
	Entity editorChild = scene.GetWorld().CreateEntity("EditorChild");
	scene.SetDefaultCamera(camera);
	ASSERT_EQ(scene.GetWorld().SetParent(coreChild, meshEntity), EcsError::None);
	ASSERT_EQ(scene.GetWorld().SetParent(editorChild, editorRoot), EcsError::None);

	ComponentId transformId = scene.GetWorld().GetSystemId("Transform");
	ComponentId meshId = scene.GetWorld().GetSystemId("Mesh");
	ComponentId visualId = scene.GetWorld().GetSystemId("Visual");
	ComponentId cameraId = scene.GetWorld().GetSystemId("Camera");
	ComponentId highlightId = scene.GetWorld().GetSystemId("Highlight");
	ComponentId editorTagId = scene.GetWorld().GetSystemId("EditorOnlyTag");
	ASSERT_NE(transformId, kInvalidComponentId);
	ASSERT_NE(meshId, kInvalidComponentId);
	ASSERT_NE(visualId, kInvalidComponentId);
	ASSERT_NE(cameraId, kInvalidComponentId);
	ASSERT_NE(highlightId, kInvalidComponentId);
	ASSERT_NE(editorTagId, kInvalidComponentId);

	Transform cameraTransform;
	cameraTransform.translation = Vec3(1.0f, 2.0f, 3.0f);
	scene.GetWorld().Transforms().Add(camera, cameraTransform);
	Transform meshTransform;
	meshTransform.translation = Vec3(4.0f, 5.0f, 6.0f);
	meshTransform.scale = Vec3(2.0f, 3.0f, 4.0f);
	scene.GetWorld().Transforms().Add(meshEntity, meshTransform);
	scene.GetWorld().Transforms().Add(coreChild, Transform{});
	scene.GetWorld().Transforms().Add(editorRoot, Transform{});
	scene.GetWorld().Transforms().Add(editorChild, Transform{});
	scene.GetWorld().UpdateTransforms();

	MeshComponent mesh;
	mesh.meshId = 77u;
	scene.GetWorld().GetSystem(meshId).AddRaw(meshEntity, &mesh);
	scene.GetWorld().GetSystem(meshId).AddRaw(editorChild, &mesh);
	VisualComponent visual;
	visual.materialId = 88u;
	scene.GetWorld().GetSystem(visualId).AddRaw(meshEntity, &visual);
	CameraComponent cameraComponent;
	cameraComponent.fovY = 0.5f;
	cameraComponent.aspect = 1.5f;
	scene.GetWorld().GetSystem(cameraId).AddRaw(camera, &cameraComponent);
	HighlightComponent highlight;
	highlight.priority = 9;
	scene.GetWorld().GetSystem(highlightId).AddRaw(meshEntity, &highlight);
	EditorOnlyTag editorTag;
	scene.GetWorld().GetSystem(editorTagId).AddRaw(editorRoot, &editorTag);

	Scene instance = scene.Instantiate();

	EXPECT_EQ(instance.GetName(), "Authored");
	EXPECT_EQ(instance.GetDefaultCamera(), camera);
	EXPECT_EQ(instance.Behaviors(), scene.Behaviors());
	EXPECT_EQ(instance.GetWorld().GetSystemId("Transform"), transformId);
	EXPECT_EQ(instance.GetWorld().GetSystemId("Mesh"), meshId);
	EXPECT_EQ(instance.GetWorld().GetSystemId("Visual"), visualId);
	EXPECT_EQ(instance.GetWorld().GetSystemId("Camera"), cameraId);
	EXPECT_EQ(instance.GetWorld().GetSystemId("Highlight"), highlightId);
	EXPECT_EQ(instance.GetWorld().GetSystemId("EditorOnlyTag"), editorTagId);

	EXPECT_TRUE(instance.GetWorld().IsValid(camera));
	EXPECT_TRUE(instance.GetWorld().IsValid(meshEntity));
	EXPECT_TRUE(instance.GetWorld().IsValid(coreChild));
	EXPECT_FALSE(instance.GetWorld().IsValid(editorRoot));
	EXPECT_FALSE(instance.GetWorld().IsValid(editorChild));
	EXPECT_EQ(instance.GetWorld().GetEntityCount(), 3u);
	EXPECT_EQ(instance.GetWorld().GetNameComponent(camera).name, "Camera");
	EXPECT_EQ(instance.GetWorld().GetNameComponent(meshEntity).name, "Mesh");
	EXPECT_EQ(instance.GetWorld().GetNameComponent(coreChild).name, "CoreChild");
	EXPECT_EQ(instance.GetWorld().GetParent(coreChild), meshEntity);

	EXPECT_NE(&instance.GetWorld().Transforms(), &scene.GetWorld().Transforms());
	EXPECT_NEAR(instance.GetWorld().Transforms().GetWorld(camera)[3][0], 1.0f, 1e-5f);
	EXPECT_NEAR(instance.GetWorld().Transforms().GetLocal(meshEntity).translation.x, 4.0f,
		1e-5f);
	EXPECT_NEAR(instance.GetWorld().Transforms().GetLocal(meshEntity).translation.y, 5.0f,
		1e-5f);
	EXPECT_NEAR(instance.GetWorld().Transforms().GetLocal(meshEntity).translation.z, 6.0f,
		1e-5f);
	EXPECT_NEAR(instance.GetWorld().Transforms().GetLocal(meshEntity).scale.x, 2.0f, 1e-5f);
	EXPECT_NEAR(instance.GetWorld().Transforms().GetLocal(meshEntity).scale.y, 3.0f, 1e-5f);
	EXPECT_NEAR(instance.GetWorld().Transforms().GetLocal(meshEntity).scale.z, 4.0f, 1e-5f);

	MeshComponent clonedMesh;
	std::memcpy(&clonedMesh, instance.GetWorld().GetSystem(meshId).GetRaw(meshEntity),
		sizeof(MeshComponent));
	EXPECT_EQ(clonedMesh.meshId, 77u);
	VisualComponent clonedVisual;
	std::memcpy(&clonedVisual, instance.GetWorld().GetSystem(visualId).GetRaw(meshEntity),
		sizeof(VisualComponent));
	EXPECT_EQ(clonedVisual.materialId, 88u);
	CameraComponent clonedCamera;
	std::memcpy(&clonedCamera, instance.GetWorld().GetSystem(cameraId).GetRaw(camera),
		sizeof(CameraComponent));
	EXPECT_FLOAT_EQ(clonedCamera.fovY, 0.5f);
	EXPECT_FLOAT_EQ(clonedCamera.aspect, 1.5f);

	EXPECT_EQ(instance.GetWorld().GetSystem(highlightId).Layer(), SystemLayer::Editor);
	EXPECT_EQ(instance.GetWorld().GetSystem(highlightId).Size(), 0u);
	EXPECT_FALSE(instance.GetWorld().GetSystem(highlightId).Has(meshEntity));
	EXPECT_EQ(instance.GetWorld().GetSystem(editorTagId).Size(), 0u);

	scene.SetName("SourceMutated");
	scene.GetWorld().SetName(camera, "SourceCamera");
	scene.GetWorld().GetSystem(meshId).Remove(meshEntity);
	scene.GetWorld().DestroyEntity(coreChild);

	EXPECT_EQ(instance.GetName(), "Authored");
	EXPECT_EQ(instance.GetWorld().GetNameComponent(camera).name, "Camera");
	EXPECT_TRUE(instance.GetWorld().GetSystem(meshId).Has(meshEntity));
	EXPECT_TRUE(instance.GetWorld().IsValid(coreChild));

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
