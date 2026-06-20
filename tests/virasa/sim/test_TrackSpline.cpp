#include <gtest/gtest.h>

#include "virasa/sim/TrackSpline.h"

#include "glm/geometric.hpp"

using namespace virasa::math;
using namespace virasa::sim;

namespace
{

void ExpectVec3Near(const Vec3& actual, const Vec3& expected, float tolerance = 1.0e-5f)
{
	EXPECT_NEAR(actual.x, expected.x, tolerance);
	EXPECT_NEAR(actual.y, expected.y, tolerance);
	EXPECT_NEAR(actual.z, expected.z, tolerance);
}

} // namespace

TEST(TrackSpline, test_track_spline_holds_authored_control_loop)
{
	TrackSpline spline;
	EXPECT_TRUE(spline.closed);
	EXPECT_TRUE(spline.controlPoints.empty());

	spline.controlPoints = {
		{Vec3(0.0f, 0.0f, 0.0f), 8.0f, 0.0f},
		{Vec3(10.0f, 0.0f, 1.0f), 9.0f, 0.1f},
		{Vec3(10.0f, 10.0f, 2.0f), 10.0f, 0.2f},
		{Vec3(0.0f, 10.0f, 3.0f), 11.0f, 0.3f},
	};

	EXPECT_EQ(SegmentCount(spline), 4U);

	for (uint32_t i = 0; i < spline.controlPoints.size(); ++i)
	{
		const TrackFrame frame = Sample(spline, static_cast<float>(i));
		ExpectVec3Near(frame.position, spline.controlPoints[i].position);
		EXPECT_FLOAT_EQ(frame.width, spline.controlPoints[i].width);
		EXPECT_FLOAT_EQ(frame.bank, spline.controlPoints[i].bank);
	}

	const TrackFrame loopFrame = Sample(spline, 4.0f);
	ExpectVec3Near(loopFrame.position, spline.controlPoints[0].position);
	EXPECT_FLOAT_EQ(loopFrame.width, spline.controlPoints[0].width);
	EXPECT_FLOAT_EQ(loopFrame.bank, spline.controlPoints[0].bank);
}

TEST(TrackSpline, test_track_spline_uses_centripetal_catmull_rom)
{
	TrackSpline spline;
	spline.controlPoints = {
		{Vec3(0.0f, 0.0f, 0.0f), 10.0f, 0.0f},
		{Vec3(2.0f, 0.0f, 0.0f), 12.0f, 0.1f},
		{Vec3(3.0f, 4.0f, 0.0f), 18.0f, 0.3f},
		{Vec3(9.0f, 4.0f, 0.0f), 20.0f, 0.5f},
	};

	const TrackFrame frame = Sample(spline, 1.5f);

	ExpectVec3Near(frame.position, Vec3(2.4127688f, 1.9318919f, 0.0f));
	EXPECT_FLOAT_EQ(frame.width, 15.0f);
	EXPECT_FLOAT_EQ(frame.bank, 0.2f);
}

TEST(TrackSpline, test_track_spline_sample_builds_frame)
{
	TrackSpline spline;
	spline.controlPoints = {
		{Vec3(0.0f, 0.0f, 0.0f), 10.0f, 0.0f},
		{Vec3(0.0f, 10.0f, 0.0f), 10.0f, 0.0f},
		{Vec3(0.0f, 20.0f, 0.0f), 10.0f, 0.0f},
		{Vec3(0.0f, 30.0f, 0.0f), 10.0f, 0.0f},
	};

	const TrackFrame frame = Sample(spline, 1.5f);

	ExpectVec3Near(frame.position, Vec3(0.0f, 15.0f, 0.0f));
	ExpectVec3Near(frame.forward, Vec3(0.0f, 1.0f, 0.0f));
	ExpectVec3Near(frame.right, Vec3(1.0f, 0.0f, 0.0f));
	ExpectVec3Near(frame.up, Vec3(0.0f, 0.0f, 1.0f));

	EXPECT_NEAR(glm::length(frame.forward), 1.0f, 1.0e-5f);
	EXPECT_NEAR(glm::length(frame.right), 1.0f, 1.0e-5f);
	EXPECT_NEAR(glm::length(frame.up), 1.0f, 1.0e-5f);
	EXPECT_NEAR(glm::dot(frame.forward, frame.right), 0.0f, 1.0e-5f);
	EXPECT_NEAR(glm::dot(frame.forward, frame.up), 0.0f, 1.0e-5f);
	EXPECT_NEAR(glm::dot(frame.right, frame.up), 0.0f, 1.0e-5f);
}

TEST(TrackSpline, test_track_spline_approx_length_sums_chords)
{
	TrackSpline spline;
	spline.controlPoints = {
		{Vec3(0.0f, 0.0f, 0.0f), 10.0f, 0.0f},
		{Vec3(10.0f, 0.0f, 0.0f), 10.0f, 0.0f},
		{Vec3(10.0f, 10.0f, 0.0f), 10.0f, 0.0f},
		{Vec3(0.0f, 10.0f, 0.0f), 10.0f, 0.0f},
	};

	const float chordLength = ApproxLength(spline, 1);
	const float refinedLength = ApproxLength(spline, 4);
	const float defaultLength = ApproxLength(spline);

	EXPECT_FLOAT_EQ(chordLength, 40.0f);
	EXPECT_GE(refinedLength, chordLength);
	EXPECT_GE(defaultLength, refinedLength);
	ExpectVec3Near(Sample(spline, 4.0f).position, Sample(spline, 0.0f).position);
}
