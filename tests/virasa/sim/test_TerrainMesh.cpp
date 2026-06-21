#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>

#include "glm/geometric.hpp"
#include "virasa/sim/TerrainMesh.h"

using namespace virasa::math;
using namespace virasa::sim;

namespace
{

void ExpectVec2Near(const Vec2& actual, const Vec2& expected, float tolerance = 1.0e-5f)
{
	EXPECT_NEAR(actual.x, expected.x, tolerance);
	EXPECT_NEAR(actual.y, expected.y, tolerance);
}

void ExpectVec3Near(const Vec3& actual, const Vec3& expected, float tolerance = 1.0e-5f)
{
	EXPECT_NEAR(actual.x, expected.x, tolerance);
	EXPECT_NEAR(actual.y, expected.y, tolerance);
	EXPECT_NEAR(actual.z, expected.z, tolerance);
}

void ExpectVec4Near(const Vec4& actual, const Vec4& expected, float tolerance = 1.0e-5f)
{
	EXPECT_NEAR(actual.x, expected.x, tolerance);
	EXPECT_NEAR(actual.y, expected.y, tolerance);
	EXPECT_NEAR(actual.z, expected.z, tolerance);
	EXPECT_NEAR(actual.w, expected.w, tolerance);
}

TerrainHeightField BuildFlatField()
{
	TerrainHeightField field;
	field.cols = 3U;
	field.rows = 3U;
	field.cellSize = 2.0f;
	field.origin = Vec2(-4.0f, 10.0f);
	field.heights = {
		7.0f,
		7.0f,
		7.0f,
		7.0f,
		7.0f,
		7.0f,
		7.0f,
		7.0f,
		7.0f,
	};
	return field;
}

TerrainHeightField BuildSlopedField()
{
	TerrainHeightField field;
	field.cols = 3U;
	field.rows = 3U;
	field.cellSize = 2.0f;
	field.origin = Vec2(-4.0f, 10.0f);
	field.heights = {
		0.0f,
		2.0f,
		4.0f,
		0.0f,
		2.0f,
		4.0f,
		0.0f,
		2.0f,
		4.0f,
	};
	return field;
}

void ExpectTriangleWindsCcw(
	const virasa::MeshData& mesh, uint32_t i0, uint32_t i1, uint32_t i2, const Vec3& normal)
{
	const Vec3& p0 = mesh.vertices[i0].position;
	const Vec3& p1 = mesh.vertices[i1].position;
	const Vec3& p2 = mesh.vertices[i2].position;
	EXPECT_GT(glm::dot(glm::cross(p1 - p0, p2 - p0), normal), 0.0f);
}

} // namespace

TEST(TerrainMesh, test_terrain_mesh_tessellates_height_grid)
{
	const TerrainHeightField flatField = BuildFlatField();
	const virasa::MeshData flatMesh = GenerateTerrainMesh(flatField);

	ASSERT_EQ(
		flatMesh.vertices.size(), static_cast<std::size_t>(flatField.cols * flatField.rows));

	for (uint32_t r = 0U; r < flatField.rows; ++r)
	{
		for (uint32_t c = 0U; c < flatField.cols; ++c)
		{
			const std::size_t vertexIndex = static_cast<std::size_t>(r * flatField.cols + c);
			const virasa::Vertex& vertex = flatMesh.vertices[vertexIndex];

			ExpectVec3Near(vertex.position,
				Vec3(flatField.origin.x + static_cast<float>(c) * flatField.cellSize,
					flatField.origin.y + static_cast<float>(r) * flatField.cellSize,
					NodeHeight(flatField, c, r)));
			EXPECT_FLOAT_EQ(vertex.normal.x, 0.0f);
			EXPECT_FLOAT_EQ(vertex.normal.y, 0.0f);
			EXPECT_FLOAT_EQ(vertex.normal.z, 1.0f);
			ExpectVec4Near(vertex.tangent, Vec4(1.0f, 0.0f, 0.0f, 1.0f));
			ExpectVec2Near(vertex.uv,
				Vec2(static_cast<float>(c) / static_cast<float>(flatField.cols - 1U),
					static_cast<float>(r) / static_cast<float>(flatField.rows - 1U)));
		}
	}

	const TerrainHeightField slopedField = BuildSlopedField();
	const virasa::MeshData slopedMesh = GenerateTerrainMesh(slopedField);
	const Vec3 expectedSlopedNormal = glm::normalize(Vec3(-1.0f, 0.0f, 1.0f));

	ASSERT_EQ(slopedMesh.vertices.size(),
		static_cast<std::size_t>(slopedField.cols * slopedField.rows));
	for (const virasa::Vertex& vertex : slopedMesh.vertices)
	{
		ExpectVec3Near(vertex.normal, expectedSlopedNormal);
		EXPECT_GT(vertex.normal.z, 0.0f);
	}
}

TEST(TerrainMesh, test_terrain_mesh_winds_ccw_front_up)
{
	const TerrainHeightField field = BuildSlopedField();
	const virasa::MeshData mesh = GenerateTerrainMesh(field);

	ASSERT_EQ(mesh.vertices.size(), static_cast<std::size_t>(field.cols * field.rows));
	ASSERT_EQ(mesh.indices.size(),
		static_cast<std::size_t>(6U * (field.cols - 1U) * (field.rows - 1U)));

	for (uint32_t r = 0U; r + 1U < field.rows; ++r)
	{
		for (uint32_t c = 0U; c + 1U < field.cols; ++c)
		{
			const uint32_t v00 = r * field.cols + c;
			const uint32_t v10 = r * field.cols + (c + 1U);
			const uint32_t v01 = (r + 1U) * field.cols + c;
			const uint32_t v11 = (r + 1U) * field.cols + (c + 1U);
			const std::size_t indexOffset =
				static_cast<std::size_t>((r * (field.cols - 1U) + c) * 6U);

			EXPECT_EQ(mesh.indices[indexOffset + 0U], v00);
			EXPECT_EQ(mesh.indices[indexOffset + 1U], v10);
			EXPECT_EQ(mesh.indices[indexOffset + 2U], v11);
			EXPECT_EQ(mesh.indices[indexOffset + 3U], v00);
			EXPECT_EQ(mesh.indices[indexOffset + 4U], v11);
			EXPECT_EQ(mesh.indices[indexOffset + 5U], v01);

			ExpectTriangleWindsCcw(mesh, v00, v10, v11, Vec3(0.0f, 0.0f, 1.0f));
			ExpectTriangleWindsCcw(mesh, v00, v11, v01, Vec3(0.0f, 0.0f, 1.0f));
		}
	}

	for (const uint32_t index : mesh.indices)
	{
		EXPECT_LT(index, field.cols * field.rows);
	}
}
