#include <gtest/gtest.h>

#include "virasa/renderer/geometry/Primitives.h"
#include "virasa/renderer/Types.h"

#include "glm/geometric.hpp"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>

namespace
{

constexpr float kEpsilon = 1e-5f;
constexpr float kPi = 3.14159265358979323846f;

bool NearlyEqual(float a, float b, float epsilon = kEpsilon)
{
	return std::fabs(a - b) <= epsilon;
}

bool NearlyEqualVec2(const glm::vec2& a, const glm::vec2& b, float epsilon = kEpsilon)
{
	return NearlyEqual(a.x, b.x, epsilon) && NearlyEqual(a.y, b.y, epsilon);
}

bool NearlyEqualVec3(const glm::vec3& a, const glm::vec3& b, float epsilon = kEpsilon)
{
	return NearlyEqual(a.x, b.x, epsilon) && NearlyEqual(a.y, b.y, epsilon) &&
		NearlyEqual(a.z, b.z, epsilon);
}

void ExpectVertexMatches(
	const virasa::Vertex& vertex,
	const glm::vec3& expectedPosition,
	const glm::vec3& expectedNormal,
	const glm::vec2& expectedUv)
{
	EXPECT_TRUE(NearlyEqualVec3(vertex.position, expectedPosition));
	EXPECT_TRUE(NearlyEqualVec3(vertex.normal, expectedNormal));
	EXPECT_TRUE(NearlyEqualVec2(vertex.uv, expectedUv));
}

} // namespace

TEST(Primitives, test_primitives_are_pure_cpu_geometry_generators)
{
	const virasa::MeshData cube = virasa::CreateCube();
	const virasa::MeshData sphere = virasa::CreateSphere();

	EXPECT_FALSE(cube.vertices.empty());
	EXPECT_FALSE(cube.indices.empty());
	EXPECT_FALSE(sphere.vertices.empty());
	EXPECT_FALSE(sphere.indices.empty());

	for (const virasa::Vertex& vertex : cube.vertices)
	{
		EXPECT_NEAR(glm::length(vertex.normal), 1.0f, kEpsilon);
		EXPECT_GE(vertex.uv.x, 0.0f);
		EXPECT_LE(vertex.uv.x, 1.0f);
		EXPECT_GE(vertex.uv.y, 0.0f);
		EXPECT_LE(vertex.uv.y, 1.0f);
	}

	for (std::size_t i = 0; i < cube.indices.size(); i += 3)
	{
		ASSERT_LT(cube.indices[i], cube.vertices.size());
		ASSERT_LT(cube.indices[i + 1], cube.vertices.size());
		ASSERT_LT(cube.indices[i + 2], cube.vertices.size());

		const virasa::Vertex& a = cube.vertices[cube.indices[i]];
		const virasa::Vertex& b = cube.vertices[cube.indices[i + 1]];
		const virasa::Vertex& c = cube.vertices[cube.indices[i + 2]];
		const glm::vec3 faceCross = glm::cross(b.position - a.position, c.position - a.position);
		const glm::vec3 faceNormal = glm::normalize(faceCross);
		const glm::vec3 averageNormal = glm::normalize(a.normal + b.normal + c.normal);
		EXPECT_GT(glm::dot(faceNormal, averageNormal), 0.99f);
	}

	for (const virasa::Vertex& vertex : sphere.vertices)
	{
		EXPECT_NEAR(glm::length(vertex.normal), 1.0f, kEpsilon);
		EXPECT_NEAR(glm::length(vertex.position), 0.5f, 1e-4f);
		EXPECT_TRUE(NearlyEqualVec3(vertex.normal, glm::normalize(vertex.position), 1e-4f));
		EXPECT_GE(vertex.uv.x, 0.0f);
		EXPECT_LE(vertex.uv.x, 1.0f);
		EXPECT_GE(vertex.uv.y, 0.0f);
		EXPECT_LE(vertex.uv.y, 1.0f);
	}

	for (std::size_t i = 0; i < sphere.indices.size(); i += 3)
	{
		ASSERT_LT(sphere.indices[i], sphere.vertices.size());
		ASSERT_LT(sphere.indices[i + 1], sphere.vertices.size());
		ASSERT_LT(sphere.indices[i + 2], sphere.vertices.size());

		const virasa::Vertex& a = sphere.vertices[sphere.indices[i]];
		const virasa::Vertex& b = sphere.vertices[sphere.indices[i + 1]];
		const virasa::Vertex& c = sphere.vertices[sphere.indices[i + 2]];
		const glm::vec3 faceCross = glm::cross(b.position - a.position, c.position - a.position);
		if (glm::length(faceCross) < 1e-6f)
		{
			continue; // degenerate pole triangle — skip winding check
		}
		const glm::vec3 triangleCenter = (a.position + b.position + c.position) / 3.0f;
		EXPECT_GT(glm::dot(faceCross, triangleCenter), 0.0f);
	}
}

TEST(Primitives, test_create_cube_generates_box_geometry)
{
	constexpr float kHalfExtent = 2.0f;
	const virasa::MeshData cube = virasa::CreateCube(kHalfExtent);

	ASSERT_EQ(cube.vertices.size(), 24u);
	ASSERT_EQ(cube.indices.size(), 36u);

	const std::array<uint32_t, 6> expectedFaceIndices = {
		0u, 1u, 2u, 0u, 2u, 3u
	};

	for (std::size_t face = 0; face < 6; ++face)
	{
		for (std::size_t i = 0; i < expectedFaceIndices.size(); ++i)
		{
			EXPECT_EQ(cube.indices[face * expectedFaceIndices.size() + i],
				cube.indices[face * expectedFaceIndices.size()] + expectedFaceIndices[i]);
		}
	}

	const std::array<glm::vec2, 4> expectedUvs = {
		glm::vec2(0.0f, 0.0f),
		glm::vec2(1.0f, 0.0f),
		glm::vec2(1.0f, 1.0f),
		glm::vec2(0.0f, 1.0f)
	};

	const std::array<glm::vec3, 6> expectedNormals = {
		glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec3(-1.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		glm::vec3(0.0f, -1.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 1.0f),
		glm::vec3(0.0f, 0.0f, -1.0f)
	};

	const std::array<std::array<glm::vec3, 4>, 6> expectedPositions = {{
		// +X face
		{
			glm::vec3(kHalfExtent, -kHalfExtent, kHalfExtent),
			glm::vec3(kHalfExtent, -kHalfExtent, -kHalfExtent),
			glm::vec3(kHalfExtent, kHalfExtent, -kHalfExtent),
			glm::vec3(kHalfExtent, kHalfExtent, kHalfExtent)
		},
		// -X face
		{
			glm::vec3(-kHalfExtent, -kHalfExtent, -kHalfExtent),
			glm::vec3(-kHalfExtent, -kHalfExtent, kHalfExtent),
			glm::vec3(-kHalfExtent, kHalfExtent, kHalfExtent),
			glm::vec3(-kHalfExtent, kHalfExtent, -kHalfExtent)
		},
		// +Y face
		{
			glm::vec3(-kHalfExtent, kHalfExtent, kHalfExtent),
			glm::vec3(kHalfExtent, kHalfExtent, kHalfExtent),
			glm::vec3(kHalfExtent, kHalfExtent, -kHalfExtent),
			glm::vec3(-kHalfExtent, kHalfExtent, -kHalfExtent)
		},
		// -Y face
		{
			glm::vec3(-kHalfExtent, -kHalfExtent, -kHalfExtent),
			glm::vec3(kHalfExtent, -kHalfExtent, -kHalfExtent),
			glm::vec3(kHalfExtent, -kHalfExtent, kHalfExtent),
			glm::vec3(-kHalfExtent, -kHalfExtent, kHalfExtent)
		},
		// +Z face
		{
			glm::vec3(-kHalfExtent, -kHalfExtent, kHalfExtent),
			glm::vec3(kHalfExtent, -kHalfExtent, kHalfExtent),
			glm::vec3(kHalfExtent, kHalfExtent, kHalfExtent),
			glm::vec3(-kHalfExtent, kHalfExtent, kHalfExtent)
		},
		// -Z face
		{
			glm::vec3(kHalfExtent, -kHalfExtent, -kHalfExtent),
			glm::vec3(-kHalfExtent, -kHalfExtent, -kHalfExtent),
			glm::vec3(-kHalfExtent, kHalfExtent, -kHalfExtent),
			glm::vec3(kHalfExtent, kHalfExtent, -kHalfExtent)
		}
	}};

	std::set<float> xValues;
	std::set<float> yValues;
	std::set<float> zValues;

	for (std::size_t face = 0; face < 6; ++face)
	{
		for (std::size_t vertexIndex = 0; vertexIndex < 4; ++vertexIndex)
		{
			const virasa::Vertex& vertex = cube.vertices[face * 4 + vertexIndex];
			ExpectVertexMatches(
				vertex,
				expectedPositions[face][vertexIndex],
				expectedNormals[face],
				expectedUvs[vertexIndex]);
			xValues.insert(vertex.position.x);
			yValues.insert(vertex.position.y);
			zValues.insert(vertex.position.z);
		}
	}

	EXPECT_EQ(xValues.size(), 2u);
	EXPECT_EQ(yValues.size(), 2u);
	EXPECT_EQ(zValues.size(), 2u);
	EXPECT_TRUE(xValues.contains(-kHalfExtent));
	EXPECT_TRUE(xValues.contains(kHalfExtent));
	EXPECT_TRUE(yValues.contains(-kHalfExtent));
	EXPECT_TRUE(yValues.contains(kHalfExtent));
	EXPECT_TRUE(zValues.contains(-kHalfExtent));
	EXPECT_TRUE(zValues.contains(kHalfExtent));
}

TEST(Primitives, test_create_sphere_generates_uv_sphere_geometry)
{
	constexpr float kRadius = 1.25f;
	constexpr uint32_t kStacks = 4;
	constexpr uint32_t kSlices = 5;
	const virasa::MeshData sphere = virasa::CreateSphere(kRadius, kStacks, kSlices);

	ASSERT_EQ(sphere.vertices.size(), static_cast<std::size_t>((kStacks + 1) * (kSlices + 1)));
	ASSERT_EQ(sphere.indices.size(), static_cast<std::size_t>(kStacks * kSlices * 6));

	for (uint32_t stack = 0; stack <= kStacks; ++stack)
	{
		const float phi = static_cast<float>(stack) * kPi / static_cast<float>(kStacks);
		for (uint32_t slice = 0; slice <= kSlices; ++slice)
		{
			const float theta = static_cast<float>(slice) * 2.0f * kPi / static_cast<float>(kSlices);
			const std::size_t index = static_cast<std::size_t>(stack) * (kSlices + 1) + slice;
			const virasa::Vertex& vertex = sphere.vertices[index];

			const glm::vec3 expectedPosition(
				kRadius * std::sin(phi) * std::cos(theta),
				kRadius * std::cos(phi),
				kRadius * std::sin(phi) * std::sin(theta));
			const glm::vec3 expectedNormal = glm::normalize(expectedPosition);
			const glm::vec2 expectedUv(
				static_cast<float>(slice) / static_cast<float>(kSlices),
				static_cast<float>(stack) / static_cast<float>(kStacks));

			EXPECT_TRUE(NearlyEqualVec3(vertex.position, expectedPosition, 1e-4f));
			EXPECT_TRUE(NearlyEqualVec3(vertex.normal, expectedNormal, 1e-4f));
			EXPECT_TRUE(NearlyEqualVec2(vertex.uv, expectedUv, 1e-5f));
			EXPECT_NEAR(glm::length(vertex.position), kRadius, 1e-4f);
			EXPECT_NEAR(glm::length(vertex.normal), 1.0f, 1e-4f);
		}
	}

	for (uint32_t stack = 0; stack < kStacks; ++stack)
	{
		for (uint32_t slice = 0; slice < kSlices; ++slice)
		{
			const uint32_t a = stack * (kSlices + 1) + slice;
			const uint32_t b = a + 1;
			const uint32_t c = (stack + 1) * (kSlices + 1) + slice;
			const uint32_t d = c + 1;
			const std::size_t base = static_cast<std::size_t>(stack * kSlices + slice) * 6;

			EXPECT_EQ(sphere.indices[base + 0], a);
			EXPECT_EQ(sphere.indices[base + 1], b);
			EXPECT_EQ(sphere.indices[base + 2], c);
			EXPECT_EQ(sphere.indices[base + 3], b);
			EXPECT_EQ(sphere.indices[base + 4], d);
			EXPECT_EQ(sphere.indices[base + 5], c);
		}
	}

	for (std::size_t i = 0; i < sphere.indices.size(); i += 3)
	{
		const virasa::Vertex& a = sphere.vertices[sphere.indices[i]];
		const virasa::Vertex& b = sphere.vertices[sphere.indices[i + 1]];
		const virasa::Vertex& c = sphere.vertices[sphere.indices[i + 2]];
		const glm::vec3 faceCross = glm::cross(b.position - a.position, c.position - a.position);
		if (glm::length(faceCross) < 1e-6f)
		{
			continue; // degenerate pole triangle — skip winding check
		}
		const glm::vec3 center = (a.position + b.position + c.position) / 3.0f;
		EXPECT_GT(glm::dot(faceCross, center), 0.0f);
	}
}
