#include <gtest/gtest.h>

#include "virasa/renderer/geometry/Primitives.h"
#include "virasa/renderer/Types.h"

#include "glm/geometric.hpp"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

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

bool NearlyEqualVec4(const glm::vec4& a, const glm::vec4& b, float epsilon = kEpsilon)
{
	return NearlyEqual(a.x, b.x, epsilon) && NearlyEqual(a.y, b.y, epsilon) &&
		NearlyEqual(a.z, b.z, epsilon) && NearlyEqual(a.w, b.w, epsilon);
}

} // namespace

TEST(Primitives, test_primitives_are_pure_cpu_geometry_generators)
{
	const virasa::MeshData cubeA = virasa::CreateCube();
	const virasa::MeshData cubeB = virasa::CreateCube();
	const virasa::MeshData sphereA = virasa::CreateSphere();
	const virasa::MeshData sphereB = virasa::CreateSphere();

	EXPECT_FALSE(cubeA.vertices.empty());
	EXPECT_FALSE(cubeA.indices.empty());
	EXPECT_FALSE(sphereA.vertices.empty());
	EXPECT_FALSE(sphereA.indices.empty());

	ASSERT_EQ(cubeA.vertices.size(), cubeB.vertices.size());
	ASSERT_EQ(cubeA.indices.size(), cubeB.indices.size());
	for (std::size_t i = 0; i < cubeA.vertices.size(); ++i)
	{
		EXPECT_TRUE(NearlyEqualVec3(cubeA.vertices[i].position, cubeB.vertices[i].position));
		EXPECT_TRUE(NearlyEqualVec3(cubeA.vertices[i].normal, cubeB.vertices[i].normal));
		EXPECT_TRUE(NearlyEqualVec4(cubeA.vertices[i].tangent, cubeB.vertices[i].tangent));
		EXPECT_TRUE(NearlyEqualVec2(cubeA.vertices[i].uv, cubeB.vertices[i].uv));
	}
	EXPECT_EQ(cubeA.indices, cubeB.indices);

	ASSERT_EQ(sphereA.vertices.size(), sphereB.vertices.size());
	ASSERT_EQ(sphereA.indices.size(), sphereB.indices.size());
	for (std::size_t i = 0; i < sphereA.vertices.size(); ++i)
	{
		EXPECT_TRUE(NearlyEqualVec3(sphereA.vertices[i].position, sphereB.vertices[i].position));
		EXPECT_TRUE(NearlyEqualVec3(sphereA.vertices[i].normal, sphereB.vertices[i].normal));
		EXPECT_TRUE(NearlyEqualVec4(sphereA.vertices[i].tangent, sphereB.vertices[i].tangent));
		EXPECT_TRUE(NearlyEqualVec2(sphereA.vertices[i].uv, sphereB.vertices[i].uv));
	}
	EXPECT_EQ(sphereA.indices, sphereB.indices);

	for (const virasa::Vertex& vertex : cubeA.vertices)
	{
		EXPECT_NEAR(glm::length(vertex.normal), 1.0f, kEpsilon);
		EXPECT_NEAR(glm::length(glm::vec3(vertex.tangent)), 1.0f, kEpsilon);
		EXPECT_NEAR(glm::dot(vertex.normal, glm::vec3(vertex.tangent)), 0.0f, kEpsilon);
		EXPECT_FLOAT_EQ(vertex.tangent.w, 1.0f);
		EXPECT_GE(vertex.uv.x, 0.0f);
		EXPECT_LE(vertex.uv.x, 1.0f);
		EXPECT_GE(vertex.uv.y, 0.0f);
		EXPECT_LE(vertex.uv.y, 1.0f);
	}

	for (std::size_t i = 0; i < cubeA.indices.size(); i += 3)
	{
		ASSERT_LT(cubeA.indices[i], cubeA.vertices.size());
		ASSERT_LT(cubeA.indices[i + 1], cubeA.vertices.size());
		ASSERT_LT(cubeA.indices[i + 2], cubeA.vertices.size());

		const virasa::Vertex& a = cubeA.vertices[cubeA.indices[i]];
		const virasa::Vertex& b = cubeA.vertices[cubeA.indices[i + 1]];
		const virasa::Vertex& c = cubeA.vertices[cubeA.indices[i + 2]];
		const glm::vec3 faceCross = glm::cross(b.position - a.position, c.position - a.position);
		const glm::vec3 faceNormal = glm::normalize(faceCross);
		const glm::vec3 averageNormal = glm::normalize(a.normal + b.normal + c.normal);
		EXPECT_GT(glm::dot(faceNormal, averageNormal), 0.99f);
	}

	for (const virasa::Vertex& vertex : sphereA.vertices)
	{
		EXPECT_NEAR(glm::length(vertex.normal), 1.0f, kEpsilon);
		EXPECT_NEAR(glm::length(vertex.position), 0.5f, 1e-4f);
		EXPECT_TRUE(NearlyEqualVec3(vertex.normal, glm::normalize(vertex.position), 1e-4f));
		EXPECT_NEAR(glm::length(glm::vec3(vertex.tangent)), 1.0f, kEpsilon);
		EXPECT_NEAR(glm::dot(vertex.normal, glm::vec3(vertex.tangent)), 0.0f, 1e-4f);
		EXPECT_FLOAT_EQ(vertex.tangent.w, 1.0f);
		EXPECT_GE(vertex.uv.x, 0.0f);
		EXPECT_LE(vertex.uv.x, 1.0f);
		EXPECT_GE(vertex.uv.y, 0.0f);
		EXPECT_LE(vertex.uv.y, 1.0f);
	}

	for (std::size_t i = 0; i < sphereA.indices.size(); i += 3)
	{
		ASSERT_LT(sphereA.indices[i], sphereA.vertices.size());
		ASSERT_LT(sphereA.indices[i + 1], sphereA.vertices.size());
		ASSERT_LT(sphereA.indices[i + 2], sphereA.vertices.size());

		const virasa::Vertex& a = sphereA.vertices[sphereA.indices[i]];
		const virasa::Vertex& b = sphereA.vertices[sphereA.indices[i + 1]];
		const virasa::Vertex& c = sphereA.vertices[sphereA.indices[i + 2]];
		const glm::vec3 faceCross = glm::cross(b.position - a.position, c.position - a.position);
		if (glm::length(faceCross) < 1e-6f)
		{
			continue;
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
	const std::array<glm::vec4, 6> expectedTangents = {
		glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
		glm::vec4(0.0f, -1.0f, 0.0f, 1.0f),
		glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
		glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f)
	};
	const std::array<std::array<glm::vec3, 4>, 6> expectedPositions = {{
		{
			glm::vec3(kHalfExtent, -kHalfExtent, kHalfExtent),
			glm::vec3(kHalfExtent, -kHalfExtent, -kHalfExtent),
			glm::vec3(kHalfExtent, kHalfExtent, -kHalfExtent),
			glm::vec3(kHalfExtent, kHalfExtent, kHalfExtent)
		},
		{
			glm::vec3(-kHalfExtent, -kHalfExtent, -kHalfExtent),
			glm::vec3(-kHalfExtent, -kHalfExtent, kHalfExtent),
			glm::vec3(-kHalfExtent, kHalfExtent, kHalfExtent),
			glm::vec3(-kHalfExtent, kHalfExtent, -kHalfExtent)
		},
		{
			glm::vec3(-kHalfExtent, kHalfExtent, kHalfExtent),
			glm::vec3(kHalfExtent, kHalfExtent, kHalfExtent),
			glm::vec3(kHalfExtent, kHalfExtent, -kHalfExtent),
			glm::vec3(-kHalfExtent, kHalfExtent, -kHalfExtent)
		},
		{
			glm::vec3(-kHalfExtent, -kHalfExtent, -kHalfExtent),
			glm::vec3(kHalfExtent, -kHalfExtent, -kHalfExtent),
			glm::vec3(kHalfExtent, -kHalfExtent, kHalfExtent),
			glm::vec3(-kHalfExtent, -kHalfExtent, kHalfExtent)
		},
		{
			glm::vec3(-kHalfExtent, -kHalfExtent, kHalfExtent),
			glm::vec3(kHalfExtent, -kHalfExtent, kHalfExtent),
			glm::vec3(kHalfExtent, kHalfExtent, kHalfExtent),
			glm::vec3(-kHalfExtent, kHalfExtent, kHalfExtent)
		},
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
		for (std::size_t i = 0; i < expectedFaceIndices.size(); ++i)
		{
			EXPECT_EQ(cube.indices[face * expectedFaceIndices.size() + i],
				static_cast<uint32_t>(face * 4) + expectedFaceIndices[i]);
		}

		for (std::size_t vertexIndex = 0; vertexIndex < 4; ++vertexIndex)
		{
			const virasa::Vertex& vertex = cube.vertices[face * 4 + vertexIndex];
			EXPECT_TRUE(NearlyEqualVec3(vertex.position, expectedPositions[face][vertexIndex]));
			EXPECT_TRUE(NearlyEqualVec3(vertex.normal, expectedNormals[face]));
			EXPECT_TRUE(NearlyEqualVec4(vertex.tangent, expectedTangents[face]));
			EXPECT_TRUE(NearlyEqualVec2(vertex.uv, expectedUvs[vertexIndex]));
			EXPECT_NEAR(glm::length(vertex.normal), 1.0f, kEpsilon);
			EXPECT_NEAR(glm::length(glm::vec3(vertex.tangent)), 1.0f, kEpsilon);
			EXPECT_NEAR(glm::dot(vertex.normal, glm::vec3(vertex.tangent)), 0.0f, kEpsilon);

			const glm::vec3 bitangent =
				glm::cross(vertex.normal, glm::vec3(vertex.tangent)) * vertex.tangent.w;
			EXPECT_NEAR(glm::length(bitangent), 1.0f, kEpsilon);
			EXPECT_NEAR(glm::dot(bitangent, vertex.normal), 0.0f, kEpsilon);
			EXPECT_NEAR(glm::dot(bitangent, glm::vec3(vertex.tangent)), 0.0f, kEpsilon);

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
			const glm::vec4 expectedTangent(
				-std::sin(theta),
				0.0f,
				std::cos(theta),
				1.0f);

			EXPECT_TRUE(NearlyEqualVec3(vertex.position, expectedPosition, 1e-4f));
			EXPECT_TRUE(NearlyEqualVec3(vertex.normal, expectedNormal, 1e-4f));
			EXPECT_TRUE(NearlyEqualVec2(vertex.uv, expectedUv, 1e-5f));
			EXPECT_TRUE(NearlyEqualVec4(vertex.tangent, expectedTangent, 1e-5f));
			EXPECT_NEAR(glm::length(vertex.position), kRadius, 1e-4f);
			EXPECT_NEAR(glm::length(vertex.normal), 1.0f, 1e-4f);
			EXPECT_NEAR(glm::length(glm::vec3(vertex.tangent)), 1.0f, 1e-5f);
			EXPECT_NEAR(glm::dot(vertex.normal, glm::vec3(vertex.tangent)), 0.0f, 1e-4f);
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
			continue;
		}
		const glm::vec3 center = (a.position + b.position + c.position) / 3.0f;
		EXPECT_GT(glm::dot(faceCross, center), 0.0f);
	}
}

TEST(Primitives, test_create_cylinder_generates_cylinder_geometry)
{
	const auto expectCylinderGeometry =
		[](const virasa::MeshData& cylinder, float radius, float halfLength, uint32_t slices)
	{
		ASSERT_EQ(cylinder.vertices.size(),
			static_cast<std::size_t>(4U * (slices + 1U) + 2U));
		ASSERT_EQ(cylinder.indices.size(), static_cast<std::size_t>(12U * slices));
		EXPECT_EQ(cylinder.indices.size() % 3U, 0U);

		float minX = cylinder.vertices.front().position.x;
		float maxX = cylinder.vertices.front().position.x;
		float maxRadius = 0.0f;

		for (const virasa::Vertex& vertex : cylinder.vertices)
		{
			EXPECT_NEAR(glm::length(vertex.normal), 1.0f, 1e-5f);
			EXPECT_NEAR(glm::length(glm::vec3(vertex.tangent)), 1.0f, 1e-5f);
			EXPECT_FLOAT_EQ(vertex.tangent.w, 1.0f);

			minX = std::min(minX, vertex.position.x);
			maxX = std::max(maxX, vertex.position.x);

			const float radialDistance =
				std::sqrt(vertex.position.y * vertex.position.y + vertex.position.z * vertex.position.z);
			maxRadius = std::max(maxRadius, radialDistance);

			EXPECT_LE(std::fabs(vertex.position.x), halfLength + 1e-5f);
			EXPECT_LE(radialDistance, radius + 1e-5f);
		}

		EXPECT_NEAR(minX, -halfLength, 1e-5f);
		EXPECT_NEAR(maxX, halfLength, 1e-5f);
		EXPECT_NEAR(maxRadius, radius, 1e-5f);
	};

	constexpr uint32_t kDefaultSlices = 16;
	const virasa::MeshData defaultCylinder = virasa::CreateCylinder();
	expectCylinderGeometry(defaultCylinder, 0.5f, 0.5f, kDefaultSlices);

	constexpr float kRadius = 2.0f;
	constexpr float kHalfLength = 3.0f;
	constexpr uint32_t kSlices = 8;
	const virasa::MeshData customCylinder = virasa::CreateCylinder(kRadius, kHalfLength, kSlices);
	expectCylinderGeometry(customCylinder, kRadius, kHalfLength, kSlices);
}
