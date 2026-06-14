#include <gtest/gtest.h>

#include "virasa/sim/AssetCatalog.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

using namespace virasa::sim;

TEST(AssetCatalog, test_asset_catalog_is_bidirectional_key_id_map_per_kind)
{
	static_assert(std::is_final_v<AssetCatalog>);
	static_assert(std::is_default_constructible_v<AssetCatalog>);
	static_assert(std::is_copy_constructible_v<AssetCatalog>);
	static_assert(std::is_copy_assignable_v<AssetCatalog>);
	static_assert(std::is_move_constructible_v<AssetCatalog>);
	static_assert(std::is_move_assignable_v<AssetCatalog>);
	static_assert(std::is_same_v<std::underlying_type_t<AssetKind>, uint32_t>);
	static_assert(static_cast<uint32_t>(AssetKind::Mesh) == 0u);
	static_assert(static_cast<uint32_t>(AssetKind::Material) == 1u);
	static_assert(kInvalidAssetId == 0xFFFFFFFFu);

	AssetCatalog catalog;

	EXPECT_EQ(catalog.Size(AssetKind::Mesh), 0u);
	EXPECT_EQ(catalog.Size(AssetKind::Material), 0u);
	EXPECT_FALSE(catalog.HasKey(AssetKind::Mesh, "builtin:cube"));
	EXPECT_FALSE(catalog.HasId(AssetKind::Mesh, 3u));
	EXPECT_EQ(catalog.IdForKey(AssetKind::Mesh, "builtin:cube"), kInvalidAssetId);
	EXPECT_TRUE(catalog.KeyForId(AssetKind::Mesh, 3u).empty());

	catalog.Bind(AssetKind::Mesh, "shared:key", 3u);
	catalog.Bind(AssetKind::Material, "shared:key", 3u);

	EXPECT_EQ(catalog.IdForKey(AssetKind::Mesh, "shared:key"), 3u);
	EXPECT_EQ(catalog.IdForKey(AssetKind::Material, "shared:key"), 3u);
	EXPECT_EQ(catalog.KeyForId(AssetKind::Mesh, 3u), std::string_view("shared:key"));
	EXPECT_EQ(catalog.KeyForId(AssetKind::Material, 3u), std::string_view("shared:key"));
	EXPECT_EQ(catalog.Size(AssetKind::Mesh), 1u);
	EXPECT_EQ(catalog.Size(AssetKind::Material), 1u);

	AssetCatalog copied = catalog;
	EXPECT_EQ(copied.IdForKey(AssetKind::Mesh, "shared:key"), 3u);
	EXPECT_EQ(copied.KeyForId(AssetKind::Material, 3u), std::string_view("shared:key"));

	AssetCatalog assigned;
	assigned = catalog;
	EXPECT_EQ(assigned.Size(AssetKind::Mesh), 1u);
	EXPECT_EQ(assigned.Size(AssetKind::Material), 1u);

	AssetCatalog moved = std::move(assigned);
	EXPECT_EQ(moved.IdForKey(AssetKind::Mesh, "shared:key"), 3u);
	EXPECT_EQ(moved.IdForKey(AssetKind::Material, "shared:key"), 3u);
}

TEST(AssetCatalog, test_asset_catalog_bind_records_both_directions)
{
	AssetCatalog catalog;
	std::string transientKey = "builtin:cube";

	catalog.Bind(AssetKind::Mesh, transientKey, 7u);
	transientKey = "changed-after-bind";

	EXPECT_TRUE(catalog.HasKey(AssetKind::Mesh, "builtin:cube"));
	EXPECT_TRUE(catalog.HasId(AssetKind::Mesh, 7u));
	EXPECT_EQ(catalog.IdForKey(AssetKind::Mesh, "builtin:cube"), 7u);
	EXPECT_EQ(catalog.KeyForId(AssetKind::Mesh, 7u), std::string_view("builtin:cube"));
	EXPECT_FALSE(catalog.HasKey(AssetKind::Mesh, "changed-after-bind"));

	catalog.Bind(AssetKind::Mesh, "builtin:cube", 8u);
	EXPECT_EQ(catalog.IdForKey(AssetKind::Mesh, "builtin:cube"), 8u);
	EXPECT_EQ(catalog.KeyForId(AssetKind::Mesh, 8u), std::string_view("builtin:cube"));
	EXPECT_FALSE(catalog.HasId(AssetKind::Mesh, 7u));
	EXPECT_TRUE(catalog.KeyForId(AssetKind::Mesh, 7u).empty());
	EXPECT_EQ(catalog.Size(AssetKind::Mesh), 1u);

	catalog.Bind(AssetKind::Mesh, "builtin:sphere", 8u);
	EXPECT_FALSE(catalog.HasKey(AssetKind::Mesh, "builtin:cube"));
	EXPECT_EQ(catalog.IdForKey(AssetKind::Mesh, "builtin:cube"), kInvalidAssetId);
	EXPECT_EQ(catalog.IdForKey(AssetKind::Mesh, "builtin:sphere"), 8u);
	EXPECT_EQ(catalog.KeyForId(AssetKind::Mesh, 8u), std::string_view("builtin:sphere"));
	EXPECT_EQ(catalog.Size(AssetKind::Mesh), 1u);

	catalog.Bind(AssetKind::Material, "builtin:sphere", 8u);
	EXPECT_EQ(catalog.IdForKey(AssetKind::Material, "builtin:sphere"), 8u);
	EXPECT_EQ(catalog.KeyForId(AssetKind::Material, 8u), std::string_view("builtin:sphere"));
	EXPECT_EQ(catalog.Size(AssetKind::Mesh), 1u);
	EXPECT_EQ(catalog.Size(AssetKind::Material), 1u);
}

TEST(AssetCatalog, test_asset_catalog_lookup_resolves_keys_and_ids)
{
	AssetCatalog catalog;
	catalog.Bind(AssetKind::Mesh, "builtin:cube", 3u);
	catalog.Bind(AssetKind::Mesh, "Builtin:Cube", 4u);
	catalog.Bind(AssetKind::Material, "builtin:cube", 9u);

	EXPECT_EQ(catalog.IdForKey(AssetKind::Mesh, "builtin:cube"), 3u);
	EXPECT_EQ(catalog.IdForKey(AssetKind::Mesh, "Builtin:Cube"), 4u);
	EXPECT_EQ(catalog.IdForKey(AssetKind::Mesh, "BUILTIN:CUBE"), kInvalidAssetId);
	EXPECT_EQ(catalog.IdForKey(AssetKind::Material, "builtin:cube"), 9u);
	EXPECT_EQ(catalog.IdForKey(AssetKind::Material, "Builtin:Cube"), kInvalidAssetId);
	EXPECT_EQ(catalog.IdForKey(AssetKind::Mesh, "missing"), kInvalidAssetId);

	EXPECT_EQ(catalog.KeyForId(AssetKind::Mesh, 3u), std::string_view("builtin:cube"));
	EXPECT_EQ(catalog.KeyForId(AssetKind::Mesh, 4u), std::string_view("Builtin:Cube"));
	EXPECT_EQ(catalog.KeyForId(AssetKind::Material, 9u), std::string_view("builtin:cube"));
	EXPECT_TRUE(catalog.KeyForId(AssetKind::Mesh, 9u).empty());
	EXPECT_TRUE(catalog.KeyForId(AssetKind::Mesh, 99u).empty());

	EXPECT_TRUE(catalog.HasKey(AssetKind::Mesh, "builtin:cube"));
	EXPECT_TRUE(catalog.HasKey(AssetKind::Mesh, "Builtin:Cube"));
	EXPECT_FALSE(catalog.HasKey(AssetKind::Mesh, "BUILTIN:CUBE"));
	EXPECT_TRUE(catalog.HasId(AssetKind::Mesh, 3u));
	EXPECT_TRUE(catalog.HasId(AssetKind::Mesh, 4u));
	EXPECT_FALSE(catalog.HasId(AssetKind::Mesh, 9u));
}

TEST(AssetCatalog, test_asset_catalog_size_and_clear)
{
	AssetCatalog catalog;

	catalog.Bind(AssetKind::Mesh, "builtin:cube", 1u);
	EXPECT_EQ(catalog.Size(AssetKind::Mesh), 1u);
	EXPECT_EQ(catalog.Size(AssetKind::Material), 0u);

	catalog.Bind(AssetKind::Mesh, "builtin:sphere", 2u);
	EXPECT_EQ(catalog.Size(AssetKind::Mesh), 2u);

	catalog.Bind(AssetKind::Mesh, "builtin:sphere", 3u);
	EXPECT_EQ(catalog.Size(AssetKind::Mesh), 2u);
	EXPECT_FALSE(catalog.HasId(AssetKind::Mesh, 2u));

	catalog.Bind(AssetKind::Mesh, "builtin:cylinder", 3u);
	EXPECT_EQ(catalog.Size(AssetKind::Mesh), 2u);
	EXPECT_FALSE(catalog.HasKey(AssetKind::Mesh, "builtin:sphere"));

	catalog.Bind(AssetKind::Material, "materials:default", 3u);
	EXPECT_EQ(catalog.Size(AssetKind::Mesh), 2u);
	EXPECT_EQ(catalog.Size(AssetKind::Material), 1u);

	catalog.Clear();
	EXPECT_EQ(catalog.Size(AssetKind::Mesh), 0u);
	EXPECT_EQ(catalog.Size(AssetKind::Material), 0u);
	EXPECT_FALSE(catalog.HasKey(AssetKind::Mesh, "builtin:cube"));
	EXPECT_FALSE(catalog.HasId(AssetKind::Mesh, 1u));
	EXPECT_EQ(catalog.IdForKey(AssetKind::Mesh, "builtin:cube"), kInvalidAssetId);
	EXPECT_TRUE(catalog.KeyForId(AssetKind::Material, 3u).empty());
}
