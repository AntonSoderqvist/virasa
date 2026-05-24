#ifndef VIRASA_RENDERER_GEOMETRY_PRIMITIVES_H
#define VIRASA_RENDERER_GEOMETRY_PRIMITIVES_H

#include <cmath>
#include <cstdint>
#include <vector>

#include "virasa/math/Types.h"
#include "virasa/renderer/Types.h"

namespace virasa
{
/**
 * @brief Create CPU-side mesh data for an axis-aligned cube centered at the origin.
 * @param half_extent Half-size of the cube along each axis.
 * @return Mesh data containing 24 vertices and 36 indices for the generated cube.
 */
[[nodiscard]] inline MeshData CreateCube(float half_extent = 0.5f)
{
	MeshData meshData;
	meshData.vertices.reserve(24);
	meshData.indices.reserve(36);

	const float h = half_extent;

	const auto appendFace = [&meshData](const glm::vec3& v0,
						const glm::vec3& v1,
						const glm::vec3& v2,
						const glm::vec3& v3,
						const glm::vec3& normal)
	{
		const uint32_t baseIndex = static_cast<uint32_t>(meshData.vertices.size());

		meshData.vertices.push_back(Vertex{v0, normal, glm::vec2(0.0f, 0.0f)});
		meshData.vertices.push_back(Vertex{v1, normal, glm::vec2(1.0f, 0.0f)});
		meshData.vertices.push_back(Vertex{v2, normal, glm::vec2(1.0f, 1.0f)});
		meshData.vertices.push_back(Vertex{v3, normal, glm::vec2(0.0f, 1.0f)});

		meshData.indices.push_back(baseIndex + 0);
		meshData.indices.push_back(baseIndex + 1);
		meshData.indices.push_back(baseIndex + 2);
		meshData.indices.push_back(baseIndex + 0);
		meshData.indices.push_back(baseIndex + 2);
		meshData.indices.push_back(baseIndex + 3);
	};

	appendFace(glm::vec3(h, -h, h),
		glm::vec3(h, -h, -h),
		glm::vec3(h, h, -h),
		glm::vec3(h, h, h),
		glm::vec3(1.0f, 0.0f, 0.0f));

	appendFace(glm::vec3(-h, -h, -h),
		glm::vec3(-h, -h, h),
		glm::vec3(-h, h, h),
		glm::vec3(-h, h, -h),
		glm::vec3(-1.0f, 0.0f, 0.0f));

	appendFace(glm::vec3(-h, h, h),
		glm::vec3(h, h, h),
		glm::vec3(h, h, -h),
		glm::vec3(-h, h, -h),
		glm::vec3(0.0f, 1.0f, 0.0f));

	appendFace(glm::vec3(-h, -h, -h),
		glm::vec3(h, -h, -h),
		glm::vec3(h, -h, h),
		glm::vec3(-h, -h, h),
		glm::vec3(0.0f, -1.0f, 0.0f));

	appendFace(glm::vec3(-h, -h, h),
		glm::vec3(h, -h, h),
		glm::vec3(h, h, h),
		glm::vec3(-h, h, h),
		glm::vec3(0.0f, 0.0f, 1.0f));

	appendFace(glm::vec3(h, -h, -h),
		glm::vec3(-h, -h, -h),
		glm::vec3(-h, h, -h),
		glm::vec3(h, h, -h),
		glm::vec3(0.0f, 0.0f, -1.0f));

	return meshData;
}

/**
 * @brief Create CPU-side mesh data for a UV sphere centered at the origin.
 * @param radius Sphere radius.
 * @param stacks Number of latitude bands.
 * @param slices Number of longitude segments.
 * @return Mesh data containing the generated UV sphere vertices and indices.
 */
[[nodiscard]] inline MeshData CreateSphere(
	float radius = 0.5f, uint32_t stacks = 16, uint32_t slices = 16)
{
	MeshData meshData;
	meshData.vertices.reserve(static_cast<size_t>(stacks + 1) * static_cast<size_t>(slices + 1));
	meshData.indices.reserve(static_cast<size_t>(stacks) * static_cast<size_t>(slices) * 6U);

	constexpr float kPi = 3.14159265358979323846f;
	constexpr float kTwoPi = 6.28318530717958647692f;

	for (uint32_t stack = 0; stack <= stacks; ++stack)
	{
		const float v = static_cast<float>(stack) / static_cast<float>(stacks);
		const float phi = v * kPi;
		const float sinPhi = std::sin(phi);
		const float cosPhi = std::cos(phi);

		for (uint32_t slice = 0; slice <= slices; ++slice)
		{
			const float u = static_cast<float>(slice) / static_cast<float>(slices);
			const float theta = u * kTwoPi;
			const float sinTheta = std::sin(theta);
			const float cosTheta = std::cos(theta);

			const glm::vec3 normal(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
			const glm::vec3 position = normal * radius;

			meshData.vertices.push_back(
				Vertex{position, glm::normalize(normal), glm::vec2(u, v)});
		}
	}

	const uint32_t ringStride = slices + 1;
	for (uint32_t stack = 0; stack < stacks; ++stack)
	{
		for (uint32_t slice = 0; slice < slices; ++slice)
		{
			const uint32_t a = stack * ringStride + slice;
			const uint32_t b = a + 1;
			const uint32_t c = (stack + 1) * ringStride + slice;
			const uint32_t d = c + 1;

			meshData.indices.push_back(a);
			meshData.indices.push_back(b);
			meshData.indices.push_back(c);

			meshData.indices.push_back(b);
			meshData.indices.push_back(d);
			meshData.indices.push_back(c);
		}
	}

	return meshData;
}
} // namespace virasa

#endif