// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#ifndef VIRASA_SIM_TERRAINMESH_H
#define VIRASA_SIM_TERRAINMESH_H

#include "virasa/math/Types.h"
#include "virasa/renderer/Types.h"
#include "virasa/sim/TerrainHeightField.h"

#include "glm/geometric.hpp"

#include <cstdint>
#include <vector>

namespace virasa::sim
{

/**
 * @brief Generate a triangulated world-space mesh from a terrain height field.
 * @param field Authored regularly-spaced terrain elevation grid.
 * @return Mesh data containing one vertex per grid node and two triangles per cell.
 */
[[nodiscard]] inline virasa::MeshData GenerateTerrainMesh(const TerrainHeightField& field)
{
	virasa::MeshData meshData;

	meshData.vertices.reserve(field.cols * field.rows);
	meshData.indices.reserve(6U * (field.cols - 1U) * (field.rows - 1U));

	for (uint32_t r = 0U; r < field.rows; ++r)
	{
		for (uint32_t c = 0U; c < field.cols; ++c)
		{
			const uint32_t cE = c + 1U < field.cols ? c + 1U : field.cols - 1U;
			const uint32_t cW = c > 0U ? c - 1U : 0U;
			const uint32_t rN = r + 1U < field.rows ? r + 1U : field.rows - 1U;
			const uint32_t rS = r > 0U ? r - 1U : 0U;
			const float dxw = static_cast<float>(cE - cW) * field.cellSize;
			const float dyw = static_cast<float>(rN - rS) * field.cellSize;
			const float dzdx = (NodeHeight(field, cE, r) - NodeHeight(field, cW, r)) / dxw;
			const float dzdy = (NodeHeight(field, c, rN) - NodeHeight(field, c, rS)) / dyw;

			meshData.vertices.push_back(virasa::Vertex{
				virasa::math::Vec3(
					field.origin.x + static_cast<float>(c) * field.cellSize,
					field.origin.y + static_cast<float>(r) * field.cellSize,
					NodeHeight(field, c, r)),
				glm::normalize(virasa::math::Vec3(-dzdx, -dzdy, 1.0f)),
				virasa::math::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
				virasa::math::Vec2(
					static_cast<float>(c) / static_cast<float>(field.cols - 1U),
					static_cast<float>(r) / static_cast<float>(field.rows - 1U)),
			});
		}
	}

	for (uint32_t r = 0U; r + 1U < field.rows; ++r)
	{
		for (uint32_t c = 0U; c + 1U < field.cols; ++c)
		{
			const uint32_t v00 = r * field.cols + c;
			const uint32_t v10 = r * field.cols + (c + 1U);
			const uint32_t v01 = (r + 1U) * field.cols + c;
			const uint32_t v11 = (r + 1U) * field.cols + (c + 1U);

			meshData.indices.push_back(v00);
			meshData.indices.push_back(v10);
			meshData.indices.push_back(v11);
			meshData.indices.push_back(v00);
			meshData.indices.push_back(v11);
			meshData.indices.push_back(v01);
		}
	}

	return meshData;
}

} // namespace virasa::sim

#endif // VIRASA_SIM_TERRAINMESH_H
