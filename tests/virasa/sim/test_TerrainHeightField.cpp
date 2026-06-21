#include <gtest/gtest.h>

#include "virasa/sim/TerrainHeightField.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

using namespace virasa::math;
using namespace virasa::sim;

namespace
{

TerrainHeightField BuildHeightField()
{
	TerrainHeightField field;
	field.heights = {
		1.0f, 2.0f, 4.0f,
		3.0f, 5.0f, 9.0f,
		6.0f, 7.0f, 8.0f,
	};
	field.cols = 3U;
	field.rows = 3U;
	field.cellSize = 2.0f;
	field.origin = Vec2(10.0f, 20.0f);
	return field;
}

} // namespace

TEST(TerrainHeightField, test_terrain_height_field_holds_authored_grid)
{
	EXPECT_TRUE(std::is_default_constructible_v<TerrainHeightField>);
	EXPECT_TRUE(std::is_copy_constructible_v<TerrainHeightField>);
	EXPECT_TRUE(std::is_copy_assignable_v<TerrainHeightField>);
	EXPECT_TRUE(std::is_move_constructible_v<TerrainHeightField>);
	EXPECT_TRUE(std::is_move_assignable_v<TerrainHeightField>);

	EXPECT_LT(offsetof(TerrainHeightField, heights), offsetof(TerrainHeightField, cols));
	EXPECT_LT(offsetof(TerrainHeightField, cols), offsetof(TerrainHeightField, rows));
	EXPECT_LT(offsetof(TerrainHeightField, rows), offsetof(TerrainHeightField, cellSize));
	EXPECT_LT(offsetof(TerrainHeightField, cellSize), offsetof(TerrainHeightField, origin));

	const TerrainHeightField defaults;
	EXPECT_TRUE(defaults.heights.empty());
	EXPECT_EQ(defaults.cols, 0U);
	EXPECT_EQ(defaults.rows, 0U);
	EXPECT_NEAR(defaults.cellSize, 1.0f, 1.0e-5f);
	EXPECT_NEAR(defaults.origin.x, 0.0f, 1.0e-5f);
	EXPECT_NEAR(defaults.origin.y, 0.0f, 1.0e-5f);

	const TerrainHeightField field = BuildHeightField();
	ASSERT_EQ(field.heights.size(), static_cast<std::size_t>(field.cols * field.rows));
	EXPECT_NEAR(field.heights[0U], 1.0f, 1.0e-5f);
	EXPECT_NEAR(field.heights[1U], 2.0f, 1.0e-5f);
	EXPECT_NEAR(field.heights[2U], 4.0f, 1.0e-5f);
	EXPECT_NEAR(field.heights[3U], 3.0f, 1.0e-5f);
	EXPECT_NEAR(field.heights[7U], 7.0f, 1.0e-5f);
}

TEST(TerrainHeightField, test_terrain_height_field_node_height_reads_grid)
{
	const TerrainHeightField field = BuildHeightField();

	EXPECT_NEAR(NodeHeight(field, 0U, 0U), 1.0f, 1.0e-5f);
	EXPECT_NEAR(NodeHeight(field, 2U, 0U), 4.0f, 1.0e-5f);
	EXPECT_NEAR(NodeHeight(field, 0U, 1U), 3.0f, 1.0e-5f);
	EXPECT_NEAR(NodeHeight(field, 1U, 2U), 7.0f, 1.0e-5f);
	EXPECT_NEAR(NodeHeight(field, 2U, 2U), 8.0f, 1.0e-5f);
}

TEST(TerrainHeightField, test_terrain_height_field_sample_bilinear)
{
	const TerrainHeightField field = BuildHeightField();

	EXPECT_NEAR(SampleHeight(field, 10.0f, 20.0f), 1.0f, 1.0e-5f);
	EXPECT_NEAR(SampleHeight(field, 14.0f, 24.0f), 8.0f, 1.0e-5f);
	EXPECT_NEAR(SampleHeight(field, 12.0f, 22.0f), 5.0f, 1.0e-5f);

	EXPECT_NEAR(SampleHeight(field, 11.0f, 21.0f), 2.75f, 1.0e-5f);
	EXPECT_NEAR(SampleHeight(field, 13.0f, 21.0f), 5.0f, 1.0e-5f);

	EXPECT_NEAR(SampleHeight(field, -100.0f, -100.0f), 1.0f, 1.0e-5f);
	EXPECT_NEAR(SampleHeight(field, 100.0f, 100.0f), 8.0f, 1.0e-5f);
	EXPECT_NEAR(SampleHeight(field, 11.0f, -100.0f), 1.5f, 1.0e-5f);
	EXPECT_NEAR(SampleHeight(field, 100.0f, 21.0f), 6.5f, 1.0e-5f);
}

TEST(TerrainHeightField, test_terrain_height_field_min_max_heights)
{
	const TerrainHeightField field = BuildHeightField();

	EXPECT_NEAR(MinHeight(field), 1.0f, 1.0e-5f);
	EXPECT_NEAR(MaxHeight(field), 9.0f, 1.0e-5f);

	TerrainHeightField flat;
	flat.heights = {4.0f, 4.0f, 4.0f, 4.0f};
	flat.cols = 2U;
	flat.rows = 2U;

	EXPECT_NEAR(MinHeight(flat), 4.0f, 1.0e-5f);
	EXPECT_NEAR(MaxHeight(flat), 4.0f, 1.0e-5f);
}
