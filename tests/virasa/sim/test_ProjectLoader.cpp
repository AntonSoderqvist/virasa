#include <gtest/gtest.h>

#include "virasa/sim/ProjectLoader.h"

#include "virasa/ecs/Components.h"
#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/sim/Builtins.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace virasa::ecs;
using namespace virasa::sim;

namespace
{

struct TempProjectRoot
{
	explicit TempProjectRoot(std::string_view suffix)
		: path(std::filesystem::temp_directory_path() / ("virasa_" + std::string(suffix)))
	{
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
		std::filesystem::create_directories(path);
	}

	~TempProjectRoot()
	{
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}

	TempProjectRoot(const TempProjectRoot&) = delete;
	TempProjectRoot& operator=(const TempProjectRoot&) = delete;

	std::filesystem::path path;
};

struct ResolveCall
{
	AssetKind kind = AssetKind::Mesh;
	std::string path;
};

class RecordingAssetResolver final : public AssetResolver
{
public:
	explicit RecordingAssetResolver(std::vector<uint32_t> idsToReturn)
		: ids(std::move(idsToReturn))
	{
	}

	[[nodiscard]] uint32_t Resolve(AssetKind kind, std::string_view path) override
	{
		calls.push_back(ResolveCall{kind, std::string(path)});
		if (calls.size() <= ids.size())
		{
			return ids[calls.size() - 1u];
		}

		return kInvalidAssetId;
	}

	std::vector<uint32_t> ids;
	std::vector<ResolveCall> calls;
};

ComponentCodecRegistry MakeCodecRegistry()
{
	ComponentCodecRegistry registry;
	RegisterBuiltinComponentCodecs(registry);
	return registry;
}

AssetCatalog MakeAuthoringCatalog(uint32_t meshId = 42u, uint32_t materialId = 9u)
{
	AssetCatalog catalog;
	catalog.Bind(AssetKind::Mesh, "meshes:cube", meshId);
	catalog.Bind(AssetKind::Material, "materials:test", materialId);
	return catalog;
}

template <typename T>
void AddComponent(World& world, Entity entity, const char* name, const T& component)
{
	const ComponentId id = world.GetSystemId(name);
	ASSERT_NE(id, kInvalidComponentId);
	(void)world.GetSystem(id).AddRaw(entity, &component);
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

Scene MakeSceneWithAssetComponents(uint32_t meshId = 42u, uint32_t materialId = 9u)
{
	Scene scene;
	scene.SetName("Project Loader Fixture");
	scene.AddBehavior("Spin");

	World& world = scene.GetWorld();
	RegisterGameplayComponents(world);

	const Entity root = world.CreateEntity("Root");
	const Entity child = world.CreateEntity("Asset Child");
	EXPECT_EQ(world.SetParent(child, root), EcsError::None);

	MeshComponent mesh;
	mesh.meshId = meshId;
	AddComponent(world, child, "Mesh", mesh);

	VisualComponent visual;
	visual.materialId = materialId;
	AddComponent(world, child, "Visual", visual);

	return scene;
}

ProjectManifest MakeManifest()
{
	ProjectManifest manifest;
	manifest.name = "Project Loader Test";
	manifest.startupScene = "scenes/main.scene.json";
	manifest.assets = {
		ProjectAssetEntry{AssetKind::Mesh, "meshes:cube", "assets/meshes/cube.mesh"},
		ProjectAssetEntry{AssetKind::Material, "materials:test", "assets/materials/test.mat"},
	};
	return manifest;
}

bool WriteTextFile(const std::filesystem::path& path, std::string_view text)
{
	std::filesystem::create_directories(path.parent_path());
	std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
	file.write(text.data(), static_cast<std::streamsize>(text.size()));
	return file.good();
}

void WriteProjectFiles(
	const std::filesystem::path& root,
	const ProjectManifest& manifest,
	const Scene& scene,
	const ComponentCodecRegistry& codecs,
	const AssetCatalog& catalog)
{
	ASSERT_TRUE(WriteTextFile(root / "project.json", SerializeManifest(manifest)));
	ASSERT_TRUE(WriteTextFile(root / manifest.startupScene, SerializeScene(scene, codecs, catalog)));
}

void ExpectManifestEquivalent(const ProjectManifest& actual, const ProjectManifest& expected)
{
	EXPECT_EQ(actual.name, expected.name);
	EXPECT_EQ(actual.startupScene, expected.startupScene);
	ASSERT_EQ(actual.assets.size(), expected.assets.size());

	for (size_t i = 0u; i < expected.assets.size(); ++i)
	{
		EXPECT_EQ(actual.assets[i].kind, expected.assets[i].kind);
		EXPECT_EQ(actual.assets[i].key, expected.assets[i].key);
		EXPECT_EQ(actual.assets[i].source, expected.assets[i].source);
	}
}

void ExpectProjectSceneEquivalent(
	const Scene& loaded,
	uint32_t expectedMeshId,
	uint32_t expectedMaterialId)
{
	EXPECT_EQ(loaded.GetName(), "Project Loader Fixture");
	EXPECT_EQ(loaded.Behaviors(), std::vector<std::string>{"Spin"});

	const World& world = loaded.GetWorld();
	const Entity root = world.FindEntityByName("Root");
	const Entity child = world.FindEntityByName("Asset Child");
	ASSERT_TRUE(world.IsValid(root));
	ASSERT_TRUE(world.IsValid(child));
	EXPECT_EQ(world.GetEntityCount(), 2u);
	EXPECT_EQ(world.GetParent(child), root);

	const MeshComponent mesh = ReadComponent<MeshComponent>(world, child, "Mesh");
	EXPECT_EQ(mesh.meshId, expectedMeshId);

	const VisualComponent visual = ReadComponent<VisualComponent>(world, child, "Visual");
	EXPECT_EQ(visual.materialId, expectedMaterialId);
}

} // namespace

TEST(
	asset_resolver_is_abstract_path_to_id_seam,
	test_asset_resolver_is_abstract_path_to_id_seam)
{
	static_assert(std::is_abstract_v<AssetResolver>);
	static_assert(std::has_virtual_destructor_v<AssetResolver>);
	static_assert(!std::is_copy_constructible_v<AssetResolver>);
	static_assert(!std::is_copy_assignable_v<AssetResolver>);
	static_assert(!std::is_move_constructible_v<AssetResolver>);
	static_assert(!std::is_move_assignable_v<AssetResolver>);

	TempProjectRoot root("project_loader_asset_resolver");
	ComponentCodecRegistry codecs = MakeCodecRegistry();
	const ProjectManifest manifest = MakeManifest();
	const Scene scene = MakeSceneWithAssetComponents();
	const AssetCatalog authoringCatalog = MakeAuthoringCatalog();
	WriteProjectFiles(root.path, manifest, scene, codecs, authoringCatalog);

	RecordingAssetResolver resolver({101u, kInvalidAssetId});
	AssetCatalog loadedCatalog;
	std::optional<LoadedProject> loaded =
		OpenProject(root.path.string(), resolver, codecs, loadedCatalog);

	ASSERT_TRUE(loaded.has_value());
	ASSERT_EQ(resolver.calls.size(), 2u);
	EXPECT_EQ(resolver.calls[0].kind, AssetKind::Mesh);
	EXPECT_EQ(resolver.calls[0].path, (root.path / manifest.assets[0].source).string());
	EXPECT_TRUE(std::filesystem::path(resolver.calls[0].path).is_absolute());
	EXPECT_EQ(resolver.calls[1].kind, AssetKind::Material);
	EXPECT_EQ(resolver.calls[1].path, (root.path / manifest.assets[1].source).string());
	EXPECT_TRUE(std::filesystem::path(resolver.calls[1].path).is_absolute());

	EXPECT_TRUE(loadedCatalog.HasKey(AssetKind::Mesh, "meshes:cube"));
	EXPECT_EQ(loadedCatalog.IdForKey(AssetKind::Mesh, "meshes:cube"), 101u);
	EXPECT_FALSE(loadedCatalog.HasKey(AssetKind::Material, "materials:test"));
}

TEST(
	open_project_reads_root_binds_catalog_and_loads_scene,
	test_open_project_reads_root_binds_catalog_and_loads_scene)
{
	TempProjectRoot root("project_loader_open_project");
	ComponentCodecRegistry codecs = MakeCodecRegistry();
	const ProjectManifest manifest = MakeManifest();
	const Scene scene = MakeSceneWithAssetComponents();
	const AssetCatalog authoringCatalog = MakeAuthoringCatalog();
	WriteProjectFiles(root.path, manifest, scene, codecs, authoringCatalog);

	RecordingAssetResolver resolver({500u, 600u});
	AssetCatalog loadedCatalog;
	std::optional<LoadedProject> loaded =
		OpenProject(root.path.string(), resolver, codecs, loadedCatalog);

	ASSERT_TRUE(loaded.has_value());
	ExpectManifestEquivalent(loaded->manifest, manifest);
	EXPECT_EQ(loadedCatalog.IdForKey(AssetKind::Mesh, "meshes:cube"), 500u);
	EXPECT_EQ(loadedCatalog.IdForKey(AssetKind::Material, "materials:test"), 600u);
	ExpectProjectSceneEquivalent(loaded->scene, 500u, 600u);

	TempProjectRoot missingManifestRoot("project_loader_missing_manifest");
	AssetCatalog missingManifestCatalog;
	RecordingAssetResolver missingManifestResolver({});
	EXPECT_FALSE(OpenProject(
		missingManifestRoot.path.string(),
		missingManifestResolver,
		codecs,
		missingManifestCatalog).has_value());

	TempProjectRoot invalidManifestRoot("project_loader_invalid_manifest");
	ASSERT_TRUE(WriteTextFile(invalidManifestRoot.path / "project.json", "not json"));
	AssetCatalog invalidManifestCatalog;
	RecordingAssetResolver invalidManifestResolver({});
	EXPECT_FALSE(OpenProject(
		invalidManifestRoot.path.string(),
		invalidManifestResolver,
		codecs,
		invalidManifestCatalog).has_value());

	TempProjectRoot missingSceneRoot("project_loader_missing_scene");
	ASSERT_TRUE(WriteTextFile(missingSceneRoot.path / "project.json", SerializeManifest(manifest)));
	AssetCatalog missingSceneCatalog;
	RecordingAssetResolver missingSceneResolver({500u, 600u});
	EXPECT_FALSE(OpenProject(
		missingSceneRoot.path.string(),
		missingSceneResolver,
		codecs,
		missingSceneCatalog).has_value());
}

TEST(
	save_project_writes_manifest_and_scene_under_root,
	test_save_project_writes_manifest_and_scene_under_root)
{
	TempProjectRoot root("project_loader_save_project");
	std::filesystem::remove_all(root.path);

	ComponentCodecRegistry codecs = MakeCodecRegistry();
	const ProjectManifest manifest = MakeManifest();
	const Scene scene = MakeSceneWithAssetComponents();
	const AssetCatalog authoringCatalog = MakeAuthoringCatalog();

	ASSERT_TRUE(SaveProject(root.path.string(), manifest, scene, codecs, authoringCatalog));
	EXPECT_TRUE(std::filesystem::is_regular_file(root.path / "project.json"));
	EXPECT_TRUE(std::filesystem::is_directory(root.path / "scenes"));
	EXPECT_TRUE(std::filesystem::is_regular_file(root.path / manifest.startupScene));

	RecordingAssetResolver resolver({42u, 9u});
	AssetCatalog loadedCatalog;
	std::optional<LoadedProject> loaded =
		OpenProject(root.path.string(), resolver, codecs, loadedCatalog);

	ASSERT_TRUE(loaded.has_value());
	ExpectManifestEquivalent(loaded->manifest, manifest);
	ExpectProjectSceneEquivalent(loaded->scene, 42u, 9u);
	EXPECT_EQ(loadedCatalog.IdForKey(AssetKind::Mesh, "meshes:cube"), 42u);
	EXPECT_EQ(loadedCatalog.IdForKey(AssetKind::Material, "materials:test"), 9u);
}
