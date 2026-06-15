#include <gtest/gtest.h>

#include "virasa/sim/ProjectManifest.h"

#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

using namespace virasa::sim;

namespace
{

constexpr unsigned kExpectedFormatVersion = 1u;

void ExpectEntryEq(
	const ProjectAssetEntry& actual,
	AssetKind expectedKind,
	const std::string& expectedKey,
	const std::string& expectedSource)
{
	EXPECT_EQ(actual.kind, expectedKind);
	EXPECT_EQ(actual.key, expectedKey);
	EXPECT_EQ(actual.source, expectedSource);
}

ProjectManifest MakeAuthoredManifest()
{
	ProjectManifest manifest;
	manifest.name = "Sample Project";
	manifest.startupScene = "scenes/start.scene.json";
	manifest.assets.push_back(
		ProjectAssetEntry{AssetKind::Mesh, "level:rock", "assets/meshes/rock.glb"});
	manifest.assets.push_back(ProjectAssetEntry{
		AssetKind::Material,
		"level:rock-material",
		"assets/materials/rock.material.json"});
	return manifest;
}

void ExpectAuthoredManifestEq(const ProjectManifest& actual, const ProjectManifest& expected)
{
	EXPECT_EQ(actual.name, expected.name);
	EXPECT_EQ(actual.startupScene, expected.startupScene);
	ASSERT_EQ(actual.assets.size(), expected.assets.size());

	for (std::size_t i = 0; i < expected.assets.size(); ++i)
	{
		ExpectEntryEq(
			actual.assets[i],
			expected.assets[i].kind,
			expected.assets[i].key,
			expected.assets[i].source);
	}
}

} // namespace

TEST(ProjectManifest, test_project_manifest_is_root_relative_project_index)
{
	static_assert(std::is_default_constructible_v<ProjectManifest>);
	static_assert(std::is_copy_constructible_v<ProjectManifest>);
	static_assert(std::is_copy_assignable_v<ProjectManifest>);
	static_assert(std::is_move_constructible_v<ProjectManifest>);
	static_assert(std::is_move_assignable_v<ProjectManifest>);
	static_assert(offsetof(ProjectManifest, name) < offsetof(ProjectManifest, startupScene));
	static_assert(offsetof(ProjectManifest, startupScene) < offsetof(ProjectManifest, assets));

	ProjectManifest manifest;
	EXPECT_TRUE(manifest.name.empty());
	EXPECT_TRUE(manifest.startupScene.empty());
	EXPECT_TRUE(manifest.assets.empty());

	manifest.name = "Moved Project";
	manifest.startupScene = "scenes/entry.scene.json";
	manifest.assets.push_back(
		ProjectAssetEntry{AssetKind::Mesh, "world:terrain", "assets/terrain.glb"});

	const nlohmann::json json = SerializeManifestToJson(manifest);
	ASSERT_TRUE(json.is_object());
	EXPECT_EQ(json["formatVersion"], kExpectedFormatVersion);
	EXPECT_EQ(json["name"], "Moved Project");
	EXPECT_EQ(json["startupScene"], "scenes/entry.scene.json");
	ASSERT_TRUE(json["assets"].is_array());
	ASSERT_EQ(json["assets"].size(), 1u);
	EXPECT_EQ(json["assets"][0]["source"], "assets/terrain.glb");
}

TEST(ProjectManifest, test_project_asset_entry_pairs_stable_key_to_relative_source)
{
	static_assert(std::is_default_constructible_v<ProjectAssetEntry>);
	static_assert(std::is_copy_constructible_v<ProjectAssetEntry>);
	static_assert(std::is_copy_assignable_v<ProjectAssetEntry>);
	static_assert(std::is_move_constructible_v<ProjectAssetEntry>);
	static_assert(std::is_move_assignable_v<ProjectAssetEntry>);
	static_assert(offsetof(ProjectAssetEntry, kind) < offsetof(ProjectAssetEntry, key));
	static_assert(offsetof(ProjectAssetEntry, key) < offsetof(ProjectAssetEntry, source));

	ProjectAssetEntry entry;
	EXPECT_EQ(entry.kind, AssetKind::Mesh);
	EXPECT_TRUE(entry.key.empty());
	EXPECT_TRUE(entry.source.empty());

	entry.kind = AssetKind::Material;
	entry.key = "shared:key";
	entry.source = "assets/materials/shared.material.json";

	const ProjectAssetEntry copied = entry;
	ExpectEntryEq(
		copied,
		AssetKind::Material,
		"shared:key",
		"assets/materials/shared.material.json");

	ProjectManifest manifest;
	manifest.assets.push_back(
		ProjectAssetEntry{AssetKind::Mesh, "shared:key", "assets/meshes/shared.glb"});
	manifest.assets.push_back(copied);

	const nlohmann::json json = SerializeManifestToJson(manifest);
	ASSERT_TRUE(json["assets"].is_array());
	ASSERT_EQ(json["assets"].size(), 2u);
	EXPECT_EQ(json["assets"][0]["kind"], "mesh");
	EXPECT_EQ(json["assets"][0]["key"], "shared:key");
	EXPECT_EQ(json["assets"][0]["source"], "assets/meshes/shared.glb");
	EXPECT_EQ(json["assets"][1]["kind"], "material");
	EXPECT_EQ(json["assets"][1]["key"], "shared:key");
	EXPECT_EQ(json["assets"][1]["source"], "assets/materials/shared.material.json");

	std::optional<ProjectManifest> loaded = DeserializeManifestFromJson(json);
	ASSERT_TRUE(loaded.has_value());
	ASSERT_EQ(loaded->assets.size(), 2u);
	ExpectEntryEq(loaded->assets[0], AssetKind::Mesh, "shared:key", "assets/meshes/shared.glb");
	ExpectEntryEq(
		loaded->assets[1],
		AssetKind::Material,
		"shared:key",
		"assets/materials/shared.material.json");
}

TEST(ProjectManifest, test_project_manifest_round_trips_through_json)
{
	const ProjectManifest manifest = MakeAuthoredManifest();

	const nlohmann::json json = SerializeManifestToJson(manifest);
	ASSERT_TRUE(json.is_object());
	EXPECT_EQ(json["formatVersion"], kExpectedFormatVersion);
	EXPECT_EQ(json["name"], "Sample Project");
	EXPECT_EQ(json["startupScene"], "scenes/start.scene.json");
	ASSERT_TRUE(json["assets"].is_array());
	ASSERT_EQ(json["assets"].size(), 2u);
	EXPECT_EQ(json["assets"][0]["kind"], "mesh");
	EXPECT_EQ(json["assets"][0]["key"], "level:rock");
	EXPECT_EQ(json["assets"][0]["source"], "assets/meshes/rock.glb");
	EXPECT_EQ(json["assets"][1]["kind"], "material");
	EXPECT_EQ(json["assets"][1]["key"], "level:rock-material");
	EXPECT_EQ(json["assets"][1]["source"], "assets/materials/rock.material.json");

	std::optional<ProjectManifest> loaded = DeserializeManifestFromJson(json);
	ASSERT_TRUE(loaded.has_value());
	ExpectAuthoredManifestEq(*loaded, manifest);

	const std::string text = SerializeManifest(manifest);
	std::optional<ProjectManifest> loadedFromText = DeserializeManifest(text);
	ASSERT_TRUE(loadedFromText.has_value());
	ExpectAuthoredManifestEq(*loadedFromText, manifest);

	nlohmann::json withUnknownKind = json;
	withUnknownKind["assets"].push_back(nlohmann::json{
		{"kind", "texture"},
		{"key", "level:albedo"},
		{"source", "assets/textures/albedo.png"},
	});
	loaded = DeserializeManifestFromJson(withUnknownKind);
	ASSERT_TRUE(loaded.has_value());
	ASSERT_EQ(loaded->assets.size(), 2u);
	ExpectAuthoredManifestEq(*loaded, manifest);

	EXPECT_FALSE(DeserializeManifest("this is not json").has_value());
	EXPECT_FALSE(DeserializeManifestFromJson(nlohmann::json::array()).has_value());
	EXPECT_FALSE(DeserializeManifestFromJson(nlohmann::json{{"name", "No Version"}}).has_value());
	EXPECT_FALSE(
		DeserializeManifestFromJson(nlohmann::json{
			{"formatVersion", kExpectedFormatVersion + 1u},
			{"name", "Future Version"},
		})
			.has_value());

	loaded = DeserializeManifestFromJson(nlohmann::json{{"formatVersion", kExpectedFormatVersion}});
	ASSERT_TRUE(loaded.has_value());
	EXPECT_TRUE(loaded->name.empty());
	EXPECT_TRUE(loaded->startupScene.empty());
	EXPECT_TRUE(loaded->assets.empty());
}
