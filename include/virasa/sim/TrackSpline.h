// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#ifndef VIRASA_SIM_TRACKSPLINE_H
#define VIRASA_SIM_TRACKSPLINE_H

#include "virasa/math/Types.h"

#include "glm/geometric.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace virasa::sim
{

/**
 * @brief Authored knot of a racing-track centerline.
 */
struct ControlPoint
{
public:
	/// @brief Centerline point in Z-up world space.
	virasa::math::Vec3 position = virasa::math::Vec3(0.0f, 0.0f, 0.0f);

	/// @brief Full lateral road width in world units.
	float width = 10.0f;

	/// @brief Right-handed roll about the local forward direction, in radians.
	float bank = 0.0f;
};

/**
 * @brief Authored closed centerline loop for a racing track.
 */
struct TrackSpline
{
public:
	/// @brief Authored control points for the centerline.
	std::vector<ControlPoint> controlPoints;

	/// @brief True when the final segment wraps back to the first point.
	bool closed = true;
};

/**
 * @brief Sampled centerline frame and road-surface parameters.
 */
struct TrackFrame
{
public:
	/// @brief Centerline position in Z-up world space.
	virasa::math::Vec3 position = virasa::math::Vec3(0.0f, 0.0f, 0.0f);

	/// @brief Unit tangent in the direction of increasing spline parameter.
	virasa::math::Vec3 forward = virasa::math::Vec3(0.0f, 1.0f, 0.0f);

	/// @brief Unit lateral direction after applying bank.
	virasa::math::Vec3 right = virasa::math::Vec3(1.0f, 0.0f, 0.0f);

	/// @brief Unit surface normal after applying bank.
	virasa::math::Vec3 up = virasa::math::Vec3(0.0f, 0.0f, 1.0f);

	/// @brief Full lateral road width in world units.
	float width = 10.0f;

	/// @brief Right-handed roll about forward, in radians.
	float bank = 0.0f;
};

namespace detail
{

[[nodiscard]] inline std::uint32_t WrapIndex(std::int64_t index, std::uint32_t count)
{
	const std::int64_t wrapped = index % static_cast<std::int64_t>(count);
	return static_cast<std::uint32_t>(wrapped < 0 ? wrapped + static_cast<std::int64_t>(count) : wrapped);
}

[[nodiscard]] inline virasa::math::Vec3 CatmullRomPosition(
	const virasa::math::Vec3& p0,
	const virasa::math::Vec3& p1,
	const virasa::math::Vec3& p2,
	const virasa::math::Vec3& p3,
	float u)
{
	const float t0 = 0.0f;
	const float t1 = t0 + std::pow(glm::length(p1 - p0), 0.5f);
	const float t2 = t1 + std::pow(glm::length(p2 - p1), 0.5f);
	const float t3 = t2 + std::pow(glm::length(p3 - p2), 0.5f);
	const float tt = t1 + u * (t2 - t1);

	const virasa::math::Vec3 a1 = ((t1 - tt) * p0 + (tt - t0) * p1) / (t1 - t0);
	const virasa::math::Vec3 a2 = ((t2 - tt) * p1 + (tt - t1) * p2) / (t2 - t1);
	const virasa::math::Vec3 a3 = ((t3 - tt) * p2 + (tt - t2) * p3) / (t3 - t2);
	const virasa::math::Vec3 b1 = ((t2 - tt) * a1 + (tt - t0) * a2) / (t2 - t0);
	const virasa::math::Vec3 b2 = ((t3 - tt) * a2 + (tt - t1) * a3) / (t3 - t1);
	return ((t2 - tt) * b1 + (tt - t1) * b2) / (t2 - t1);
}

[[nodiscard]] inline virasa::math::Vec3 CatmullRomDerivative(
	const virasa::math::Vec3& p0,
	const virasa::math::Vec3& p1,
	const virasa::math::Vec3& p2,
	const virasa::math::Vec3& p3,
	float u)
{
	const float t0 = 0.0f;
	const float t1 = t0 + std::pow(glm::length(p1 - p0), 0.5f);
	const float t2 = t1 + std::pow(glm::length(p2 - p1), 0.5f);
	const float t3 = t2 + std::pow(glm::length(p3 - p2), 0.5f);
	const float tt = t1 + u * (t2 - t1);

	const virasa::math::Vec3 a1 = ((t1 - tt) * p0 + (tt - t0) * p1) / (t1 - t0);
	const virasa::math::Vec3 a2 = ((t2 - tt) * p1 + (tt - t1) * p2) / (t2 - t1);
	const virasa::math::Vec3 a3 = ((t3 - tt) * p2 + (tt - t2) * p3) / (t3 - t2);

	const virasa::math::Vec3 da1 = (p1 - p0) / (t1 - t0);
	const virasa::math::Vec3 da2 = (p2 - p1) / (t2 - t1);
	const virasa::math::Vec3 da3 = (p3 - p2) / (t3 - t2);

	const virasa::math::Vec3 b1 = ((t2 - tt) * a1 + (tt - t0) * a2) / (t2 - t0);
	const virasa::math::Vec3 b2 = ((t3 - tt) * a2 + (tt - t1) * a3) / (t3 - t1);

	const virasa::math::Vec3 db1 = (-a1 + (t2 - tt) * da1 + a2 + (tt - t0) * da2) / (t2 - t0);
	const virasa::math::Vec3 db2 = (-a2 + (t3 - tt) * da2 + a3 + (tt - t1) * da3) / (t3 - t1);

	return (-b1 + (t2 - tt) * db1 + b2 + (tt - t1) * db2) / (t2 - t1);
}

} // namespace detail

[[nodiscard]] inline uint32_t SegmentCount(const TrackSpline& spline)
{
	if (spline.closed)
	{
		return static_cast<uint32_t>(spline.controlPoints.size());
	}

	return spline.controlPoints.size() > 1 ? static_cast<uint32_t>(spline.controlPoints.size() - 1) : 0U;
}

[[nodiscard]] inline TrackFrame Sample(const TrackSpline& spline, float t)
{
	const uint32_t n = SegmentCount(spline);
	const float segmentFloat = std::floor(t);
	const uint32_t seg = detail::WrapIndex(static_cast<std::int64_t>(segmentFloat), n);
	const float u = t - segmentFloat;

	const uint32_t i0 = detail::WrapIndex(static_cast<std::int64_t>(seg) - 1, n);
	const uint32_t i1 = seg;
	const uint32_t i2 = detail::WrapIndex(static_cast<std::int64_t>(seg) + 1, n);
	const uint32_t i3 = detail::WrapIndex(static_cast<std::int64_t>(seg) + 2, n);

	const ControlPoint& p0 = spline.controlPoints[i0];
	const ControlPoint& p1 = spline.controlPoints[i1];
	const ControlPoint& p2 = spline.controlPoints[i2];
	const ControlPoint& p3 = spline.controlPoints[i3];

	TrackFrame frame;
	frame.position = detail::CatmullRomPosition(p0.position, p1.position, p2.position, p3.position, u);
	frame.forward = glm::normalize(detail::CatmullRomDerivative(p0.position, p1.position, p2.position, p3.position, u));
	frame.width = p1.width + (p2.width - p1.width) * u;
	frame.bank = p1.bank + (p2.bank - p1.bank) * u;

	const virasa::math::Vec3 worldUp(0.0f, 0.0f, 1.0f);
	const virasa::math::Vec3 right0 = glm::normalize(glm::cross(frame.forward, worldUp));
	const virasa::math::Vec3 up0 = glm::cross(right0, frame.forward);
	const float c = std::cos(frame.bank);
	const float s = std::sin(frame.bank);

	frame.right = right0 * c + up0 * s;
	frame.up = up0 * c - right0 * s;
	return frame;
}

[[nodiscard]] inline float ApproxLength(const TrackSpline& spline, uint32_t samplesPerSegment = 16)
{
	const uint32_t n = SegmentCount(spline);
	const uint32_t sampleCount = n * samplesPerSegment;
	float length = 0.0f;

	virasa::math::Vec3 previous = Sample(spline, 0.0f).position;
	for (uint32_t j = 1; j <= sampleCount; ++j)
	{
		const float t = static_cast<float>(j) * (static_cast<float>(n) / static_cast<float>(sampleCount));
		const virasa::math::Vec3 current = Sample(spline, t).position;
		length += glm::length(current - previous);
		previous = current;
	}

	return length;
}

} // namespace virasa::sim

#endif // VIRASA_SIM_TRACKSPLINE_H
