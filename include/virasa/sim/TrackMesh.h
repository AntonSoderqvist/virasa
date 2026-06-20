// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#ifndef VIRASA_SIM_TRACKMESH_H
#define VIRASA_SIM_TRACKMESH_H

#include "virasa/renderer/Types.h"
#include "virasa/sim/TrackSpline.h"

#include <cstddef>
#include <cstdint>

namespace virasa::sim
{

/**
 * @brief Generate a flat ribbon mesh from a closed track spline.
 * @param spline Authored track centerline and road parameters.
 * @param segmentsPerSpan Number of sampled mesh spans per spline segment.
 * @return Mesh data containing 2 vertices and 6 indices per sampled ring.
 */
[[nodiscard]] inline virasa::MeshData GenerateTrackMesh(const TrackSpline& spline, uint32_t segmentsPerSpan)
{
	virasa::MeshData meshData;

	const uint32_t n = SegmentCount(spline);
	const uint32_t ringCount = n * segmentsPerSpan;
	if (ringCount == 0U)
	{
		return meshData;
	}

	meshData.vertices.reserve(static_cast<std::size_t>(ringCount) * 2U);
	meshData.indices.reserve(static_cast<std::size_t>(ringCount) * 6U);

	for (uint32_t j = 0; j < ringCount; ++j)
	{
		const float t =
			static_cast<float>(j) * (static_cast<float>(n) / static_cast<float>(ringCount));
		const TrackFrame frame = Sample(spline, t);
		const float halfWidth = 0.5f * frame.width;
		const virasa::math::Vec3 offset = frame.right * halfWidth;
		const virasa::math::Vec4 tangent(frame.right, 1.0f);
		const float v = static_cast<float>(j) / static_cast<float>(ringCount);

		meshData.vertices.push_back(virasa::Vertex{
			frame.position - offset,
			frame.up,
			tangent,
			virasa::math::Vec2(0.0f, v),
		});
		meshData.vertices.push_back(virasa::Vertex{
			frame.position + offset,
			frame.up,
			tangent,
			virasa::math::Vec2(1.0f, v),
		});
	}

	for (uint32_t j = 0; j < ringCount; ++j)
	{
		const uint32_t nextRing = (j + 1U) % ringCount;
		const uint32_t l0 = 2U * j;
		const uint32_t r0 = l0 + 1U;
		const uint32_t l1 = 2U * nextRing;
		const uint32_t r1 = l1 + 1U;

		meshData.indices.push_back(l0);
		meshData.indices.push_back(r0);
		meshData.indices.push_back(l1);
		meshData.indices.push_back(r0);
		meshData.indices.push_back(r1);
		meshData.indices.push_back(l1);
	}

	return meshData;
}

} // namespace virasa::sim

#endif // VIRASA_SIM_TRACKMESH_H
