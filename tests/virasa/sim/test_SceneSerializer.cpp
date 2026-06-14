#include <gtest/gtest.h>

#include "virasa/sim/SceneSerializer.h"

#include "virasa/ecs/Components.h"
#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/math/Transform.h"
#include "virasa/sim/Builtins.h"
#include "virasa/sim/GameplayComponents.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace virasa::ecs;
using namespace virasa::math;
using namespace virasa::sim;

namespace
{

struct AuthoredScene
{
	Scene scene;
	Entity root = Entity::Invalid();
	Entity child = Entity::Invalid();
	Entity camera = Entity::Invalid();
};

void ExpectVec3Eq(const Vec3& actual, const Vec3& expected)
{
	EXPECT_FLOAT_EQ(actual.x, expected.x);
	EXPECT_FLOAT_EQ(actual.y, expected.y);
	EXPECT_FLOAT_EQ(actual.z, expected.z);
}

void ExpectQuatEq(const Quat& actual, const Quat& expected)
{
	EXPECT_FLOAT_EQ(actual.w, expected.w);
	EXPECT_FLOAT_EQ(actual.x, expected.x);
	EXPECT_FLOAT_EQ(actual.y, expected.y);
	EXPECT_FLOAT_EQ(actual.z, expected.z);
}

template <typename T>
T ReadComponent(const World& world, Entity entity, const char* name)
{
	const ComponentId id = world.GetSystemId(name);
	EXPECT_NE(id, kInvalidComponentId);
	EXPECT_TRUE(world.GetSystem(id).Has(entity));

	T component;
	std::memcpy(&component, world.GetSystem(id).GetRaw(entity), sizeof(T));
	return component;
}

template <typename T>
void AddComponent(World& world, Entity entity, const char* name, const T& component)
{
	const ComponentId id = world.GetSystemId(name);
	ASSERT_NE(id, kInvalidComponentId);
	(void)world.GetSystem(id).AddRaw(entity, &component);
}

AuthoredScene MakeAuthoredScene()
{
	AuthoredScene authored;
	authored.scene.SetName("Serializer Fixture");
	authored.scene.AddBehavior("Spin");
	authored.scene.AddBehavior("UnregisteredBehavior");

	World& world = authored.scene.GetWorld();
	RegisterGameplayComponents(world);

	authored.root = world.CreateEntity("Root");
	authored.child = world.CreateEntity("Child");
	authored.camera = world.CreateEntity("Camera");
	EXPECT_EQ(world.SetParent(authored.child, authored.root), EcsError::None);
	EXPECT_EQ(world.SetParent(authored.camera, authored.root), EcsError::None);
	authored.scene.SetDefaultCamera(authored.camera);

	Transform rootTransform;
	rootTransform.translation = Vec3(1.0f, 2.0f, 3.0f);
	rootTransform.rotation = Quat(0.9238795f, 0.0f, 0.3826834f, 0.0f);
	rootTransform.scale = Vec3(2.0f, 2.5f, 3.0f);
	world.Transforms().Add(authored.root, rootTransform);

	MeshComponent mesh;
	mesh.meshId = 42u;
	AddComponent(world, authored.child, "Mesh", mesh);

	VisualComponent visual;
	visual.materialId = 9u;
	AddComponent(world, authored.child, "Visual", visual);

	SpinComponent spin;
	spin.angularVelocity = Vec3(0.25f, 0.5f, 0.75f);
	AddComponent(world, authored.child, "Spin", spin);

	CameraComponent camera;
	camera.domain = virasa::CameraDomain::Editor;
	camera.fovY = 1.1f;
	camera.aspect = 1.777f;
	camera.nearPlane = 0.2f;
	camera.farPlane = 500.0f;
	AddComponent(world, authored.camera, "Camera", camera);

	return authored;
}

ComponentCodecRegistry MakeCodecRegistry()
{
	ComponentCodecRegistry registry;
	RegisterBuiltinComponentCodecs(registry);
	return registry;
}

AssetCatalog MakeAssetCatalog()
{
	AssetCatalog catalog;
	catalog.Bind(AssetKind::Mesh, "meshes:cube", 42u);
	catalog.Bind(AssetKind::Material, "materials:test", 9u);
	return catalog;
}

void ExpectAuthoredRoundTripEquivalent(const Scene& loaded)
{
	const World& world = loaded.GetWorld();
	const Entity loadedRoot = world.FindEntityByName("Root");
	const Entity loadedChild = world.FindEntityByName("Child");
	const Entity loadedCamera = world.FindEntityByName("Camera");

	ASSERT_TRUE(world.IsValid(loadedRoot));
	ASSERT_TRUE(world.IsValid(loadedChild));
	ASSERT_TRUE(world.IsValid(loadedCamera));
	EXPECT_EQ(world.GetEntityCount(), 3u);
	EXPECT_EQ(world.GetRoots(), std::vector<Entity>{loadedRoot});
	EXPECT_EQ(world.GetParent(loadedRoot), Entity::Invalid());
	EXPECT_EQ(world.GetParent(loadedChild), loadedRoot);
	EXPECT_EQ(world.GetParent(loadedCamera), loadedRoot);
	EXPECT_EQ(world.GetChildren(loadedRoot), (std::vector<Entity>{loadedChild, loadedCamera}));
	EXPECT_EQ(loaded.GetDefaultCamera(), loadedCamera);

	EXPECT_EQ(loaded.GetName(), "Serializer Fixture");
	const std::vector<std::string> expectedBehaviors = {"Spin", "UnregisteredBehavior"};
	EXPECT_EQ(loaded.Behaviors(), expectedBehaviors);

	const Transform& transform = world.GetTransforms().GetLocal(loadedRoot);
	ExpectVec3Eq(transform.translation, Vec3(1.0f, 2.0f, 3.0f));
	ExpectQuatEq(transform.rotation, Quat(0.9238795f, 0.0f, 0.3826834f, 0.0f));
	ExpectVec3Eq(transform.scale, Vec3(2.0f, 2.5f, 3.0f));

	const MeshComponent mesh = ReadComponent<MeshComponent>(world, loadedChild, "Mesh");
	EXPECT_EQ(mesh.meshId, 42u);

	const VisualComponent visual = ReadComponent<VisualComponent>(world, loadedChild, "Visual");
	EXPECT_EQ(visual.materialId, 9u);

	const SpinComponent spin = ReadComponent<SpinComponent>(world, loadedChild, "Spin");
	ExpectVec3Eq(spin.angularVelocity, Vec3(0.25f, 0.5f, 0.75f));

	const CameraComponent camera = ReadComponent<CameraComponent>(world, loadedCamera, "Camera");
	EXPECT_EQ(camera.domain, virasa::CameraDomain::Editor);
	EXPECT_FLOAT_EQ(camera.fovY, 1.1f);
	EXPECT_FLOAT_EQ(camera.aspect, 1.777f);
	EXPECT_FLOAT_EQ(camera.nearPlane, 0.2f);
	EXPECT_FLOAT_EQ(camera.farPlane, 500.0f);
}

} // namespace

TEST(SceneSerializer, test_scene_serializer_round_trips_scene_through_json)
{
	AuthoredScene authored = MakeAuthoredScene();
	ComponentCodecRegistry registry = MakeCodecRegistry();
	AssetCatalog catalog = MakeAssetCatalog();

	const nlohmann::json json = SerializeSceneToJson(authored.scene, registry, catalog);
	ASSERT_TRUE(json.is_object());

	std::optional<Scene> loaded = DeserializeSceneFromJson(json, registry, catalog);
	ASSERT_TRUE(loaded.has_value());
	ExpectAuthoredRoundTripEquivalent(*loaded);

	const std::string text = SerializeScene(authored.scene, registry, catalog);
	std::optional<Scene> loadedFromText = DeserializeScene(text, registry, catalog);
	ASSERT_TRUE(loadedFromText.has_value());
	ExpectAuthoredRoundTripEquivalent(*loadedFromText);
}

TEST(SceneSerializer, test_scene_serializer_assigns_stable_entity_ids_and_remaps_on_load)
{
	AuthoredScene authored = MakeAuthoredScene();
	ComponentCodecRegistry registry = MakeCodecRegistry();
	AssetCatalog catalog = MakeAssetCatalog();

	nlohmann::json json = SerializeSceneToJson(authored.scene, registry, catalog);
	ASSERT_TRUE(json["entities"].is_array());
	ASSERT_EQ(json["entities"].size(), 3u);

	EXPECT_EQ(json["entities"][0]["id"], 0u);
	EXPECT_TRUE(json["entities"][0]["parent"].is_null());
	EXPECT_EQ(json["entities"][0]["name"], "Root");
	EXPECT_EQ(json["entities"][1]["id"], 1u);
	EXPECT_EQ(json["entities"][1]["parent"], 0u);
	EXPECT_EQ(json["entities"][1]["name"], "Child");
	EXPECT_EQ(json["entities"][2]["id"], 2u);
	EXPECT_EQ(json["entities"][2]["parent"], 0u);
	EXPECT_EQ(json["entities"][2]["name"], "Camera");
	EXPECT_EQ(json["defaultCamera"], 2u);

	std::optional<Scene> loaded = DeserializeSceneFromJson(json, registry, catalog);
	ASSERT_TRUE(loaded.has_value());
	const World& loadedWorld = loaded->GetWorld();
	const Entity loadedRoot = loadedWorld.FindEntityByName("Root");
	const Entity loadedChild = loadedWorld.FindEntityByName("Child");
	const Entity loadedCamera = loadedWorld.FindEntityByName("Camera");
	EXPECT_EQ(loadedWorld.GetParent(loadedChild), loadedRoot);
	EXPECT_EQ(loadedWorld.GetParent(loadedCamera), loadedRoot);
	EXPECT_EQ(loaded->GetDefaultCamera(), loadedCamera);

	json["defaultCamera"] = 99u;
	loaded = DeserializeSceneFromJson(json, registry, catalog);
	ASSERT_TRUE(loaded.has_value());
	EXPECT_EQ(loaded->GetDefaultCamera(), Entity::Invalid());
}

TEST(SceneSerializer, test_scene_serializer_walks_systems_through_codecs)
{
	AuthoredScene authored = MakeAuthoredScene();
	ComponentCodecRegistry registry = MakeCodecRegistry();
	AssetCatalog catalog = MakeAssetCatalog();

	nlohmann::json json = SerializeSceneToJson(authored.scene, registry, catalog);
	ASSERT_TRUE(json["entities"][1]["components"].contains("Mesh"));
	ASSERT_TRUE(json["entities"][1]["components"].contains("Visual"));
	ASSERT_TRUE(json["entities"][1]["components"].contains("Spin"));
	EXPECT_FALSE(json["entities"][1]["components"].contains("Name"));
	EXPECT_EQ(json["entities"][1]["components"]["Mesh"]["meshId"], "meshes:cube");
	EXPECT_EQ(json["entities"][1]["components"]["Visual"]["materialId"], "materials:test");

	json["entities"][1]["components"]["UnknownComponent"] = nlohmann::json{{"value", 5u}};
	json["entities"][1]["components"]["Mesh"]["meshId"] = "missing:mesh";
	json["entities"][1]["components"]["Visual"]["materialId"] = "missing:material";

	std::optional<Scene> loaded = DeserializeSceneFromJson(json, registry, catalog);
	ASSERT_TRUE(loaded.has_value());
	const World& world = loaded->GetWorld();
	const Entity child = world.FindEntityByName("Child");
	ASSERT_TRUE(world.IsValid(child));

	const MeshComponent mesh = ReadComponent<MeshComponent>(world, child, "Mesh");
	EXPECT_EQ(mesh.meshId, kInvalidAssetId);

	const VisualComponent visual = ReadComponent<VisualComponent>(world, child, "Visual");
	EXPECT_EQ(visual.materialId, kInvalidAssetId);

	const SpinComponent spin = ReadComponent<SpinComponent>(world, child, "Spin");
	ExpectVec3Eq(spin.angularVelocity, Vec3(0.25f, 0.5f, 0.75f));
	EXPECT_EQ(world.GetSystemId("UnknownComponent"), kInvalidComponentId);
}

TEST(SceneSerializer, test_scene_serializer_preserves_manifest_and_metadata)
{
	ComponentCodecRegistry registry = MakeCodecRegistry();
	AssetCatalog catalog = MakeAssetCatalog();

	Scene empty;
	const std::optional<Scene> loadedEmpty = DeserializeSceneFromJson(
		SerializeSceneToJson(empty, registry, catalog),
		registry,
		catalog);
	ASSERT_TRUE(loadedEmpty.has_value());
	EXPECT_TRUE(loadedEmpty->GetName().empty());
	EXPECT_TRUE(loadedEmpty->Behaviors().empty());
	EXPECT_EQ(loadedEmpty->GetDefaultCamera(), Entity::Invalid());
	EXPECT_EQ(loadedEmpty->GetWorld().GetEntityCount(), 0u);

	AuthoredScene authored = MakeAuthoredScene();
	nlohmann::json json = SerializeSceneToJson(authored.scene, registry, catalog);
	json["behaviors"].push_back("DefinitelyNotRegistered");

	const std::optional<Scene> loaded = DeserializeSceneFromJson(json, registry, catalog);
	ASSERT_TRUE(loaded.has_value());

	const std::vector<std::string> expectedBehaviors = {
		"Spin",
		"UnregisteredBehavior",
		"DefinitelyNotRegistered",
	};
	EXPECT_EQ(loaded->GetName(), "Serializer Fixture");
	EXPECT_EQ(loaded->Behaviors(), expectedBehaviors);
	EXPECT_EQ(
		loaded->GetDefaultCamera(),
		loaded->GetWorld().FindEntityByName("Camera"));
}

TEST(SceneSerializer, test_scene_serializer_deserialize_validates_document_shape)
{
	AuthoredScene authored = MakeAuthoredScene();
	ComponentCodecRegistry registry = MakeCodecRegistry();
	AssetCatalog catalog = MakeAssetCatalog();
	const nlohmann::json valid = SerializeSceneToJson(authored.scene, registry, catalog);

	EXPECT_FALSE(DeserializeScene("not json", registry, catalog).has_value());
	EXPECT_FALSE(DeserializeSceneFromJson(nlohmann::json::array(), registry, catalog).has_value());

	nlohmann::json malformed = valid;
	malformed.erase("formatVersion");
	EXPECT_FALSE(DeserializeSceneFromJson(malformed, registry, catalog).has_value());

	malformed = valid;
	malformed["formatVersion"] = 999u;
	EXPECT_FALSE(DeserializeSceneFromJson(malformed, registry, catalog).has_value());

	malformed = valid;
	malformed["entities"] = nlohmann::json::object();
	EXPECT_FALSE(DeserializeSceneFromJson(malformed, registry, catalog).has_value());

	malformed = valid;
	malformed["entities"][0].erase("id");
	EXPECT_FALSE(DeserializeSceneFromJson(malformed, registry, catalog).has_value());

	nlohmann::json tolerated = valid;
	tolerated["entities"][1]["components"]["UnknownComponent"] = nlohmann::json::object();
	tolerated["entities"][1]["components"]["Mesh"] = nlohmann::json::array();
	tolerated["entities"][1]["components"]["Visual"] = nlohmann::json::object();
	tolerated["behaviors"].push_back("MissingBehavior");

	std::optional<Scene> loaded = DeserializeSceneFromJson(tolerated, registry, catalog);
	ASSERT_TRUE(loaded.has_value());
	const Entity child = loaded->GetWorld().FindEntityByName("Child");
	ASSERT_TRUE(loaded->GetWorld().IsValid(child));
	EXPECT_FALSE(loaded->GetWorld().GetSystem(loaded->GetWorld().GetSystemId("Mesh")).Has(child));

	const VisualComponent visual = ReadComponent<VisualComponent>(
		loaded->GetWorld(),
		child,
		"Visual");
	EXPECT_EQ(visual.materialId, kInvalidAssetId);
	EXPECT_TRUE(loaded->HasBehavior("MissingBehavior"));
}
