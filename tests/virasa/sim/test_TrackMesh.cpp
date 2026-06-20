#include <gtest/gtest.h>

#include "virasa/sim/TrackMesh.h"

#include "glm/geometric.hpp"

#include <cstddef>
#include <cstdint>

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

TrackSpline BuildSquareLoop()
{
	TrackSpline spline;
	spline.controlPoints = {
		{Vec3(0.0f, 0.0f, 0.0f), 4.0f, 0.0f},
		{Vec3(10.0f, 0.0f, 0.0f), 4.0f, 0.0f},
		{Vec3(10.0f, 10.0f, 0.0f), 4.0f, 0.0f},
		{Vec3(0.0f, 10.0f, 0.0f), 4.0f, 0.0f},
	};
	return spline;
}

void ExpectTriangleWindsCcw(
	const virasa::MeshData& mesh,
	uint32_t i0,
	uint32_t i1,
	uint32_t i2,
	const Vec3& normal)
{
	const Vec3& p0 = mesh.vertices[i0].position;
	const Vec3& p1 = mesh.vertices[i1].position;
	const Vec3& p2 = mesh.vertices[i2].position;
	EXPECT_GT(glm::dot(glm::cross(p1 - p0, p2 - p0), normal), 0.0f);
}

} // namespace

TEST(TrackMesh, test_track_mesh_extrudes_ribbon_along_spline)
{
	const TrackSpline spline = BuildSquareLoop();
	const uint32_t segmentsPerSpan = 3U;
	const uint32_t n = static_cast<uint32_t>(spline.controlPoints.size());
	const uint32_t ringCount = n * segmentsPerSpan;

	const virasa::MeshData mesh = GenerateTrackMesh(spline, segmentsPerSpan);

	ASSERT_EQ(mesh.vertices.size(), static_cast<std::size_t>(2U * ringCount));
	ASSERT_EQ(mesh.indices.size(), static_cast<std::size_t>(6U * ringCount));

	for (uint32_t j = 0; j < ringCount; ++j)
	{
		const float t =
			static_cast<float>(j) * (static_cast<float>(n) / static_cast<float>(ringCount));
		const TrackFrame frame = Sample(spline, t);
		const Vec3 offset = frame.right * (0.5f * frame.width);
		const float v = static_cast<float>(j) / static_cast<float>(ringCount);

		const virasa::Vertex& left = mesh.vertices[2U * j];
		const virasa::Vertex& right = mesh.vertices[2U * j + 1U];

		ExpectVec3Near(left.position, frame.position - offset);
		ExpectVec3Near(right.position, frame.position + offset);
		ExpectVec3Near(left.normal, frame.up);
		ExpectVec3Near(right.normal, frame.up);
		ExpectVec2Near(left.uv, Vec2(0.0f, v));
		ExpectVec2Near(right.uv, Vec2(1.0f, v));
	}
}

TEST(TrackMesh, test_track_mesh_closes_loop_and_winds_ccw)
{
	const TrackSpline spline = BuildSquareLoop();
	const uint32_t segmentsPerSpan = 2U;
	const uint32_t n = static_cast<uint32_t>(spline.controlPoints.size());
	const uint32_t ringCount = n * segmentsPerSpan;

	const virasa::MeshData mesh = GenerateTrackMesh(spline, segmentsPerSpan);

	ASSERT_EQ(mesh.vertices.size(), static_cast<std::size_t>(2U * ringCount));
	ASSERT_EQ(mesh.indices.size(), static_cast<std::size_t>(6U * ringCount));

	for (uint32_t j = 0; j < ringCount; ++j)
	{
		const uint32_t nextRing = (j + 1U) % ringCount;
		const uint32_t l0 = 2U * j;
		const uint32_t r0 = l0 + 1U;
		const uint32_t l1 = 2U * nextRing;
		const uint32_t r1 = l1 + 1U;
		const std::size_t indexOffset = static_cast<std::size_t>(j) * 6U;

		EXPECT_EQ(mesh.indices[indexOffset + 0U], l0);
		EXPECT_EQ(mesh.indices[indexOffset + 1U], r0);
		EXPECT_EQ(mesh.indices[indexOffset + 2U], l1);
		EXPECT_EQ(mesh.indices[indexOffset + 3U], r0);
		EXPECT_EQ(mesh.indices[indexOffset + 4U], r1);
		EXPECT_EQ(mesh.indices[indexOffset + 5U], l1);

		const float t =
			static_cast<float>(j) * (static_cast<float>(n) / static_cast<float>(ringCount));
		const TrackFrame frame = Sample(spline, t);
		ExpectTriangleWindsCcw(mesh, l0, r0, l1, frame.up);
		ExpectTriangleWindsCcw(mesh, r0, r1, l1, frame.up);
	}

	const std::size_t lastIndexOffset = static_cast<std::size_t>(ringCount - 1U) * 6U;
	EXPECT_EQ(mesh.indices[lastIndexOffset + 2U], 0U);
	EXPECT_EQ(mesh.indices[lastIndexOffset + 4U], 1U);
	EXPECT_EQ(mesh.indices[lastIndexOffset + 5U], 0U);
}
