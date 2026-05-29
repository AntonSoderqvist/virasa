#include <gtest/gtest.h>

#include "virasa/renderer/geometry/Tangents.h"
#include "virasa/renderer/Types.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>
#include <cstddef>
#include <thread>
#include <vector>

namespace
{

constexpr float kEpsilon = 1e-4f;

virasa::Vertex MakeVertex(float px, float py, float pz,
	float nx, float ny, float nz,
	float u, float v,
	float tx, float ty, float tz, float tw)
{
	virasa::Vertex vertex{};
	vertex.position = virasa::math::Vec3(px, py, pz);
	vertex.normal = virasa::math::Vec3(nx, ny, nz);
	vertex.uv = virasa::math::Vec2(u, v);
	vertex.tangent = virasa::math::Vec4(tx, ty, tz, tw);
	return vertex;
}

void ExpectVec3Near(const virasa::math::Vec3& actual, const virasa::math::Vec3& expected, float epsilon)
{
	EXPECT_NEAR(actual.x, expected.x, epsilon);
	EXPECT_NEAR(actual.y, expected.y, epsilon);
	EXPECT_NEAR(actual.z, expected.z, epsilon);
}

void ExpectVec2Near(const virasa::math::Vec2& actual, const virasa::math::Vec2& expected, float epsilon)
{
	EXPECT_NEAR(actual.x, expected.x, epsilon);
	EXPECT_NEAR(actual.y, expected.y, epsilon);
}

void ExpectVec4Near(const virasa::math::Vec4& actual, const virasa::math::Vec4& expected, float epsilon)
{
	EXPECT_NEAR(actual.x, expected.x, epsilon);
	EXPECT_NEAR(actual.y, expected.y, epsilon);
	EXPECT_NEAR(actual.z, expected.z, epsilon);
	EXPECT_NEAR(actual.w, expected.w, epsilon);
}

} // namespace

TEST(Tangents, test_generate_tangents_overwrites_per_vertex_tangent)
{
	virasa::MeshData meshData{};
	meshData.vertices = {
		MakeVertex(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 9.0f, 8.0f, 7.0f, -1.0f),
		MakeVertex(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 6.0f, 5.0f, 4.0f, -1.0f),
		MakeVertex(1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 3.0f, 2.0f, 1.0f, -1.0f),
		MakeVertex(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, -3.0f, -2.0f, -1.0f, -1.0f)
	};
	meshData.indices = {0u, 1u, 2u, 0u, 2u, 3u};

	const std::vector<virasa::math::Vec3> originalPositions = {
		meshData.vertices[0].position,
		meshData.vertices[1].position,
		meshData.vertices[2].position,
		meshData.vertices[3].position
	};
	const std::vector<virasa::math::Vec3> originalNormals = {
		meshData.vertices[0].normal,
		meshData.vertices[1].normal,
		meshData.vertices[2].normal,
		meshData.vertices[3].normal
	};
	const std::vector<virasa::math::Vec2> originalUvs = {
		meshData.vertices[0].uv,
		meshData.vertices[1].uv,
		meshData.vertices[2].uv,
		meshData.vertices[3].uv
	};
	const std::vector<uint32_t> originalIndices = meshData.indices;
	const std::size_t originalVertexCount = meshData.vertices.size();
	const std::size_t originalIndexCount = meshData.indices.size();

	virasa::GenerateTangents(meshData);

	EXPECT_EQ(meshData.vertices.size(), originalVertexCount);
	EXPECT_EQ(meshData.indices.size(), originalIndexCount);
	EXPECT_EQ(meshData.indices, originalIndices);

	for (std::size_t i = 0; i < meshData.vertices.size(); ++i)
	{
		ExpectVec3Near(meshData.vertices[i].position, originalPositions[i], kEpsilon);
		ExpectVec3Near(meshData.vertices[i].normal, originalNormals[i], kEpsilon);
		ExpectVec2Near(meshData.vertices[i].uv, originalUvs[i], kEpsilon);

		const virasa::math::Vec3 tangentXyz(
			meshData.vertices[i].tangent.x,
			meshData.vertices[i].tangent.y,
			meshData.vertices[i].tangent.z);
		const float tangentLength = glm::length(tangentXyz);
		const float tangentDotNormal = glm::dot(tangentXyz, meshData.vertices[i].normal);

		EXPECT_NEAR(tangentLength, 1.0f, kEpsilon);
		EXPECT_NEAR(tangentDotNormal, 0.0f, kEpsilon);
		EXPECT_TRUE(meshData.vertices[i].tangent.w == 1.0f || meshData.vertices[i].tangent.w == -1.0f);
		EXPECT_GT(glm::dot(tangentXyz, virasa::math::Vec3(1.0f, 0.0f, 0.0f)), 0.99f);
	}
}

TEST(Tangents, test_generate_tangents_is_pure_cpu_computation)
{
	virasa::MeshData meshDataA{};
	meshDataA.vertices = {
		MakeVertex(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f),
		MakeVertex(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f),
		MakeVertex(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f)
	};
	meshDataA.indices = {0u, 1u, 2u};

	virasa::MeshData meshDataB{};
	meshDataB.vertices = {
		MakeVertex(0.0f, 0.0f, 0.0f, 0.0f, std::sin(glm::radians(45.0f)), std::cos(glm::radians(45.0f)),
			0.0f, 0.0f, 5.0f, 5.0f, 5.0f, 5.0f),
		MakeVertex(1.0f, 0.0f, 0.0f, 0.0f, std::sin(glm::radians(45.0f)), std::cos(glm::radians(45.0f)),
			1.0f, 0.0f, 4.0f, 4.0f, 4.0f, 4.0f),
		MakeVertex(0.0f, 1.0f, 1.0f, 0.0f, std::sin(glm::radians(45.0f)), std::cos(glm::radians(45.0f)),
			0.0f, 1.0f, 3.0f, 3.0f, 3.0f, 3.0f)
	};
	meshDataB.indices = {0u, 1u, 2u};

	std::thread threadA([&meshDataA]()
	{
		virasa::GenerateTangents(meshDataA);
	});
	std::thread threadB([&meshDataB]()
	{
		virasa::GenerateTangents(meshDataB);
	});

	threadA.join();
	threadB.join();

	for (const virasa::Vertex& vertex : meshDataA.vertices)
	{
		const virasa::math::Vec3 tangentXyz(vertex.tangent.x, vertex.tangent.y, vertex.tangent.z);
		EXPECT_NEAR(glm::length(tangentXyz), 1.0f, kEpsilon);
		EXPECT_NEAR(glm::dot(tangentXyz, vertex.normal), 0.0f, kEpsilon);
		EXPECT_TRUE(vertex.tangent.w == 1.0f || vertex.tangent.w == -1.0f);
	}

	for (const virasa::Vertex& vertex : meshDataB.vertices)
	{
		const virasa::math::Vec3 tangentXyz(vertex.tangent.x, vertex.tangent.y, vertex.tangent.z);
		EXPECT_NEAR(glm::length(tangentXyz), 1.0f, kEpsilon);
		EXPECT_NEAR(glm::dot(tangentXyz, vertex.normal), 0.0f, kEpsilon);
		EXPECT_TRUE(vertex.tangent.w == 1.0f || vertex.tangent.w == -1.0f);
	}

	virasa::MeshData meshDataSequential{};
	meshDataSequential.vertices = {
		MakeVertex(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -7.0f, -7.0f, -7.0f, -7.0f),
		MakeVertex(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, -6.0f, -6.0f, -6.0f, -6.0f),
		MakeVertex(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, -5.0f, -5.0f, -5.0f, -5.0f)
	};
	meshDataSequential.indices = {0u, 1u, 2u};

	virasa::GenerateTangents(meshDataSequential);
	const std::vector<virasa::math::Vec4> firstPassTangents = {
		meshDataSequential.vertices[0].tangent,
		meshDataSequential.vertices[1].tangent,
		meshDataSequential.vertices[2].tangent
	};

	virasa::GenerateTangents(meshDataSequential);

	for (std::size_t i = 0; i < meshDataSequential.vertices.size(); ++i)
	{
		ExpectVec4Near(meshDataSequential.vertices[i].tangent, firstPassTangents[i], kEpsilon);
	}
}
