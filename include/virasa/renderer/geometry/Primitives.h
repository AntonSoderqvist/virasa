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
						const glm::vec3& normal,
						const glm::vec3& tangentXYZ)
	{
		const uint32_t baseIndex = static_cast<uint32_t>(meshData.vertices.size());
		const glm::vec4 tangent(tangentXYZ, 1.0f);

		meshData.vertices.push_back(Vertex{v0, normal, tangent, glm::vec2(0.0f, 0.0f)});
		meshData.vertices.push_back(Vertex{v1, normal, tangent, glm::vec2(1.0f, 0.0f)});
		meshData.vertices.push_back(Vertex{v2, normal, tangent, glm::vec2(1.0f, 1.0f)});
		meshData.vertices.push_back(Vertex{v3, normal, tangent, glm::vec2(0.0f, 1.0f)});

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
		glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f));

	appendFace(glm::vec3(-h, -h, -h),
		glm::vec3(-h, -h, h),
		glm::vec3(-h, h, h),
		glm::vec3(-h, h, -h),
		glm::vec3(-1.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, -1.0f, 0.0f));

	appendFace(glm::vec3(-h, h, h),
		glm::vec3(h, h, h),
		glm::vec3(h, h, -h),
		glm::vec3(-h, h, -h),
		glm::vec3(0.0f, 1.0f, 0.0f),
		glm::vec3(-1.0f, 0.0f, 0.0f));

	appendFace(glm::vec3(-h, -h, -h),
		glm::vec3(h, -h, -h),
		glm::vec3(h, -h, h),
		glm::vec3(-h, -h, h),
		glm::vec3(0.0f, -1.0f, 0.0f),
		glm::vec3(1.0f, 0.0f, 0.0f));

	appendFace(glm::vec3(-h, -h, h),
		glm::vec3(h, -h, h),
		glm::vec3(h, h, h),
		glm::vec3(-h, h, h),
		glm::vec3(0.0f, 0.0f, 1.0f),
		glm::vec3(1.0f, 0.0f, 0.0f));

	appendFace(glm::vec3(h, -h, -h),
		glm::vec3(-h, -h, -h),
		glm::vec3(-h, h, -h),
		glm::vec3(h, h, -h),
		glm::vec3(0.0f, 0.0f, -1.0f),
		glm::vec3(-1.0f, 0.0f, 0.0f));

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
			const glm::vec4 tangent(-sinTheta, 0.0f, cosTheta, 1.0f);

			meshData.vertices.push_back(
				Vertex{position, glm::normalize(normal), tangent, glm::vec2(u, v)});
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

/**
 * @brief Create CPU-side mesh data for a capped cylinder centered at the origin.
 * @param radius Cylinder radius in the local YZ plane.
 * @param half_length Half-length of the cylinder along the local X axis.
 * @param slices Number of radial segments.
 * @return Mesh data containing the generated cylinder vertices and indices.
 */
[[nodiscard]] inline MeshData CreateCylinder(
	float radius = 0.5f, float half_length = 0.5f, uint32_t slices = 16)
{
	MeshData meshData;
	meshData.vertices.reserve(static_cast<size_t>(4U) * static_cast<size_t>(slices + 1U) + 2U);
	meshData.indices.reserve(static_cast<size_t>(slices) * 12U);

	constexpr float kTwoPi = 6.28318530717958647692f;
	const uint32_t ringStride = slices + 1U;

	const uint32_t sidePositiveBase = 0U;
	const uint32_t sideNegativeBase = ringStride;
	for (uint32_t side = 0; side < 2U; ++side)
	{
		const float x = (side == 0U) ? half_length : -half_length;
		const float v = static_cast<float>(side);

		for (uint32_t slice = 0; slice <= slices; ++slice)
		{
			const float u = static_cast<float>(slice) / static_cast<float>(slices);
			const float theta = u * kTwoPi;
			const float sinTheta = std::sin(theta);
			const float cosTheta = std::cos(theta);

			const glm::vec3 normal(0.0f, cosTheta, sinTheta);
			const glm::vec3 position(x, radius * cosTheta, radius * sinTheta);
			const glm::vec4 tangent(0.0f, sinTheta, -cosTheta, 1.0f);

			meshData.vertices.push_back(Vertex{position, normal, tangent, glm::vec2(u, v)});
		}
	}

	for (uint32_t slice = 0; slice < slices; ++slice)
	{
		const uint32_t a = sidePositiveBase + slice;
		const uint32_t b = sidePositiveBase + slice + 1U;
		const uint32_t c = sideNegativeBase + slice;
		const uint32_t d = sideNegativeBase + slice + 1U;

		meshData.indices.push_back(a);
		meshData.indices.push_back(c);
		meshData.indices.push_back(b);

		meshData.indices.push_back(b);
		meshData.indices.push_back(c);
		meshData.indices.push_back(d);
	}

	const uint32_t positiveCapCenter = static_cast<uint32_t>(meshData.vertices.size());
	meshData.vertices.push_back(Vertex{glm::vec3(half_length, 0.0f, 0.0f),
		glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
		glm::vec2(0.5f, 0.5f)});
	const uint32_t positiveCapRim = static_cast<uint32_t>(meshData.vertices.size());
	for (uint32_t slice = 0; slice <= slices; ++slice)
	{
		const float u = static_cast<float>(slice) / static_cast<float>(slices);
		const float theta = u * kTwoPi;
		const float sinTheta = std::sin(theta);
		const float cosTheta = std::cos(theta);

		meshData.vertices.push_back(Vertex{glm::vec3(half_length, radius * cosTheta, radius * sinTheta),
			glm::vec3(1.0f, 0.0f, 0.0f),
			glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
			glm::vec2(0.5f + 0.5f * cosTheta, 0.5f + 0.5f * sinTheta)});
	}

	for (uint32_t slice = 0; slice < slices; ++slice)
	{
		meshData.indices.push_back(positiveCapCenter);
		meshData.indices.push_back(positiveCapRim + slice);
		meshData.indices.push_back(positiveCapRim + slice + 1U);
	}

	const uint32_t negativeCapCenter = static_cast<uint32_t>(meshData.vertices.size());
	meshData.vertices.push_back(Vertex{glm::vec3(-half_length, 0.0f, 0.0f),
		glm::vec3(-1.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, -1.0f, 0.0f, 1.0f),
		glm::vec2(0.5f, 0.5f)});
	const uint32_t negativeCapRim = static_cast<uint32_t>(meshData.vertices.size());
	for (uint32_t slice = 0; slice <= slices; ++slice)
	{
		const float u = static_cast<float>(slice) / static_cast<float>(slices);
		const float theta = u * kTwoPi;
		const float sinTheta = std::sin(theta);
		const float cosTheta = std::cos(theta);

		meshData.vertices.push_back(Vertex{glm::vec3(-half_length, radius * cosTheta, radius * sinTheta),
			glm::vec3(-1.0f, 0.0f, 0.0f),
			glm::vec4(0.0f, -1.0f, 0.0f, 1.0f),
			glm::vec2(0.5f + 0.5f * cosTheta, 0.5f + 0.5f * sinTheta)});
	}

	for (uint32_t slice = 0; slice < slices; ++slice)
	{
		meshData.indices.push_back(negativeCapCenter);
		meshData.indices.push_back(negativeCapRim + slice + 1U);
		meshData.indices.push_back(negativeCapRim + slice);
	}

	return meshData;
}
} // namespace virasa

#endif
