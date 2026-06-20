#include <gtest/gtest.h>

#include "virasa/physics/CollisionMeshRegistry.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace
{

void ExpectVec3Eq(const virasa::math::Vec3& actual, const virasa::math::Vec3& expected)
{
	EXPECT_FLOAT_EQ(actual.x, expected.x);
	EXPECT_FLOAT_EQ(actual.y, expected.y);
	EXPECT_FLOAT_EQ(actual.z, expected.z);
}

void ExpectMeshDataEq(const virasa::physics::CollisionMeshData& actual,
                      const std::vector<virasa::math::Vec3>& expectedVertices,
                      const std::vector<uint32_t>& expectedIndices)
{
	ASSERT_EQ(actual.vertices.size(), expectedVertices.size());
	for (size_t i = 0; i < expectedVertices.size(); ++i)
	{
		ExpectVec3Eq(actual.vertices[i], expectedVertices[i]);
	}

	EXPECT_EQ(actual.indices, expectedIndices);
}

} // namespace

TEST(CollisionMeshRegistry, test_collision_mesh_data_holds_triangle_soup)
{
	virasa::physics::CollisionMeshData data;
	EXPECT_TRUE(data.vertices.empty());
	EXPECT_TRUE(data.indices.empty());

	const std::vector<virasa::math::Vec3> vertices = {
		virasa::math::Vec3(0.0f, 0.0f, 0.0f),
		virasa::math::Vec3(1.0f, 0.0f, 0.0f),
		virasa::math::Vec3(0.0f, 1.0f, 0.0f),
		virasa::math::Vec3(0.0f, 0.0f, 1.0f),
	};
	const std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

	data.vertices = vertices;
	data.indices = indices;
	ExpectMeshDataEq(data, vertices, indices);

	virasa::physics::CollisionMeshData copy = data;
	ExpectMeshDataEq(copy, vertices, indices);

	virasa::physics::CollisionMeshData moved = std::move(copy);
	ExpectMeshDataEq(moved, vertices, indices);
}

TEST(CollisionMeshRegistry, test_collision_mesh_registry_assigns_dense_ids)
{
	virasa::physics::CollisionMeshRegistry registry;
	EXPECT_EQ(registry.Count(), size_t{0});
	EXPECT_FALSE(registry.Has(0));
	EXPECT_EQ(registry.Get(0), nullptr);

	const std::vector<virasa::math::Vec3> firstVertices = {
		virasa::math::Vec3(0.0f, 0.0f, 0.0f),
		virasa::math::Vec3(1.0f, 0.0f, 0.0f),
		virasa::math::Vec3(0.0f, 1.0f, 0.0f),
	};
	const std::vector<uint32_t> firstIndices = {0, 1, 2};

	const uint32_t firstId = registry.Register(firstVertices, firstIndices);
	EXPECT_EQ(firstId, uint32_t{0});
	EXPECT_EQ(registry.Count(), size_t{1});
	EXPECT_TRUE(registry.Has(firstId));

	const virasa::physics::CollisionMeshData* firstMesh = registry.Get(firstId);
	ASSERT_NE(firstMesh, nullptr);
	ExpectMeshDataEq(*firstMesh, firstVertices, firstIndices);

	const std::vector<virasa::math::Vec3> secondVertices = {
		virasa::math::Vec3(0.0f, 0.0f, 1.0f),
		virasa::math::Vec3(1.0f, 0.0f, 1.0f),
		virasa::math::Vec3(0.0f, 1.0f, 1.0f),
		virasa::math::Vec3(1.0f, 1.0f, 1.0f),
	};
	const std::vector<uint32_t> secondIndices = {0, 1, 2, 1, 3, 2};

	const uint32_t secondId = registry.Register(secondVertices, secondIndices);
	EXPECT_EQ(secondId, uint32_t{1});
	EXPECT_EQ(registry.Count(), size_t{2});
	EXPECT_TRUE(registry.Has(secondId));

	const virasa::physics::CollisionMeshData* secondMesh = registry.Get(secondId);
	ASSERT_NE(secondMesh, nullptr);
	ExpectMeshDataEq(*secondMesh, secondVertices, secondIndices);

	ASSERT_NE(firstMesh, nullptr);
	ExpectMeshDataEq(*firstMesh, firstVertices, firstIndices);

	EXPECT_FALSE(registry.Has(99));
	EXPECT_EQ(registry.Get(99), nullptr);
}
