// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#ifndef VIRASA_SIM_TERRAINHEIGHTFIELD_H
#define VIRASA_SIM_TERRAINHEIGHTFIELD_H

#include "virasa/math/Types.h"

#include <cstdint>
#include <vector>

namespace virasa::sim
{

/**
 * @brief Authored regularly-spaced terrain elevation grid.
 */
struct TerrainHeightField
{
public:
	/// @brief Row-major authored elevations in world Z units.
	std::vector<float> heights;

	/// @brief Number of grid nodes along the world +X axis.
	uint32_t cols = 0u;

	/// @brief Number of grid nodes along the world +Y axis.
	uint32_t rows = 0u;

	/// @brief Uniform world-unit spacing between adjacent grid nodes.
	float cellSize = 1.0f;

	/// @brief World XY position of the node at column 0, row 0.
	virasa::math::Vec2 origin = virasa::math::Vec2(0.0f, 0.0f);
};

[[nodiscard]] inline float NodeHeight(const TerrainHeightField& field, uint32_t col, uint32_t row)
{
	return field.heights[row * field.cols + col];
}

[[nodiscard]] inline float SampleHeight(const TerrainHeightField& field, float x, float y)
{
	float fx = (x - field.origin.x) / field.cellSize;
	float fy = (y - field.origin.y) / field.cellSize;

	const float maxX = static_cast<float>(field.cols - 1u);
	const float maxY = static_cast<float>(field.rows - 1u);
	fx = fx < 0.0f ? 0.0f : (fx > maxX ? maxX : fx);
	fy = fy < 0.0f ? 0.0f : (fy > maxY ? maxY : fy);

	const uint32_t c0 = static_cast<uint32_t>(fx);
	const uint32_t r0 = static_cast<uint32_t>(fy);
	const uint32_t c1 = c0 + 1u < field.cols ? c0 + 1u : field.cols - 1u;
	const uint32_t r1 = r0 + 1u < field.rows ? r0 + 1u : field.rows - 1u;
	const float tx = fx - static_cast<float>(c0);
	const float ty = fy - static_cast<float>(r0);

	const float h00 = NodeHeight(field, c0, r0);
	const float h10 = NodeHeight(field, c1, r0);
	const float h01 = NodeHeight(field, c0, r1);
	const float h11 = NodeHeight(field, c1, r1);
	const float hx0 = h00 + (h10 - h00) * tx;
	const float hx1 = h01 + (h11 - h01) * tx;
	return hx0 + (hx1 - hx0) * ty;
}

[[nodiscard]] inline float MinHeight(const TerrainHeightField& field)
{
	float minHeight = field.heights[0];
	for (uint32_t i = 1u; i < field.cols * field.rows; ++i)
	{
		minHeight = field.heights[i] < minHeight ? field.heights[i] : minHeight;
	}

	return minHeight;
}

[[nodiscard]] inline float MaxHeight(const TerrainHeightField& field)
{
	float maxHeight = field.heights[0];
	for (uint32_t i = 1u; i < field.cols * field.rows; ++i)
	{
		maxHeight = field.heights[i] > maxHeight ? field.heights[i] : maxHeight;
	}

	return maxHeight;
}

} // namespace virasa::sim

#endif // VIRASA_SIM_TERRAINHEIGHTFIELD_H
