#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <utility>

#include "virasa/sim/Tick.h"

using namespace virasa::sim;

namespace
{

constexpr double kEpsilon = 1e-5;
constexpr double kAlphaEpsilon = 1e-4;

} // namespace

TEST(TickContext, test_tick_context_reports_fixed_step_state)
{
	TickContext context;

	EXPECT_FLOAT_EQ(context.deltaTime, 0.0f);
	EXPECT_DOUBLE_EQ(context.totalTime, 0.0);
	EXPECT_EQ(context.tickIndex, 0u);
	EXPECT_FLOAT_EQ(context.alpha, 0.0f);
	EXPECT_FLOAT_EQ(context.timeScale, 1.0f);
	EXPECT_FALSE(context.paused);

	context.deltaTime = 0.016f;
	context.totalTime = 12.5;
	context.tickIndex = 42u;
	context.alpha = 0.75f;
	context.timeScale = 0.5f;
	context.paused = true;

	TickContext copy = context;
	EXPECT_FLOAT_EQ(copy.deltaTime, 0.016f);
	EXPECT_DOUBLE_EQ(copy.totalTime, 12.5);
	EXPECT_EQ(copy.tickIndex, 42u);
	EXPECT_FLOAT_EQ(copy.alpha, 0.75f);
	EXPECT_FLOAT_EQ(copy.timeScale, 0.5f);
	EXPECT_TRUE(copy.paused);

	TickContext moved = std::move(copy);
	EXPECT_FLOAT_EQ(moved.deltaTime, 0.016f);
	EXPECT_DOUBLE_EQ(moved.totalTime, 12.5);
	EXPECT_EQ(moved.tickIndex, 42u);
	EXPECT_FLOAT_EQ(moved.alpha, 0.75f);
	EXPECT_FLOAT_EQ(moved.timeScale, 0.5f);
	EXPECT_TRUE(moved.paused);
}

TEST(Clock, test_clock_samples_wall_time_deltas)
{
	Clock clock;

	EXPECT_GE(clock.Sample(), 0.0);

	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	EXPECT_GT(clock.Sample(), 0.0);

	clock.Reset();
	EXPECT_GE(clock.Sample(), 0.0);
}

TEST(Stepper, test_stepper_is_fixed_timestep_accumulator)
{
	Stepper stepper(0.1f, 5u);

	EXPECT_FLOAT_EQ(stepper.FixedDelta(), 0.1f);
	EXPECT_EQ(stepper.TickIndex(), 0u);
	EXPECT_DOUBLE_EQ(stepper.TotalTime(), 0.0);
	EXPECT_FLOAT_EQ(stepper.GetTimeScale(), 1.0f);
	EXPECT_FALSE(stepper.IsPaused());

	EXPECT_EQ(stepper.Advance(0.25), 2u);

	const TickContext first = stepper.NextStep();
	EXPECT_FLOAT_EQ(first.deltaTime, 0.1f);
	EXPECT_EQ(first.tickIndex, 1u);
	EXPECT_NEAR(first.totalTime, 0.1, kEpsilon);

	const TickContext second = stepper.NextStep();
	EXPECT_FLOAT_EQ(second.deltaTime, 0.1f);
	EXPECT_EQ(second.tickIndex, 2u);
	EXPECT_NEAR(second.totalTime, 0.2, kEpsilon);
	EXPECT_EQ(stepper.TickIndex(), 2u);
	EXPECT_NEAR(stepper.TotalTime(), 0.2, kEpsilon);

	const float alphaBeforePause = stepper.Alpha();
	stepper.SetPaused(true);
	EXPECT_TRUE(stepper.IsPaused());
	EXPECT_EQ(stepper.Advance(1.0), 0u);
	EXPECT_EQ(stepper.TickIndex(), 2u);
	EXPECT_NEAR(stepper.TotalTime(), 0.2, kEpsilon);
	EXPECT_NEAR(stepper.Alpha(), alphaBeforePause, kEpsilon);

	stepper.SetPaused(false);
	EXPECT_FALSE(stepper.IsPaused());

	Stepper scaled(0.1f, 5u);
	scaled.SetTimeScale(2.0f);
	EXPECT_FLOAT_EQ(scaled.GetTimeScale(), 2.0f);
	EXPECT_EQ(scaled.Advance(0.11), 2u);
}

TEST(Stepper, test_stepper_caps_steps_to_avoid_spiral)
{
	Stepper stepper(0.1f, 3u);

	EXPECT_EQ(stepper.Advance(1.0), 3u);

	[[maybe_unused]] const TickContext first = stepper.NextStep();
	[[maybe_unused]] const TickContext second = stepper.NextStep();
	[[maybe_unused]] const TickContext third = stepper.NextStep();

	EXPECT_GE(stepper.Alpha(), 0.0f);
	EXPECT_LT(stepper.Alpha(), 1.0f);
	EXPECT_EQ(stepper.Advance(0.0), 0u);
}

TEST(Stepper, test_stepper_alpha_is_render_interpolation_remainder)
{
	Stepper stepper(0.1f, 5u);

	EXPECT_EQ(stepper.Advance(0.25), 2u);
	[[maybe_unused]] const TickContext first = stepper.NextStep();
	[[maybe_unused]] const TickContext second = stepper.NextStep();

	EXPECT_NEAR(stepper.Alpha(), 0.5f, kAlphaEpsilon);
	EXPECT_GE(stepper.Alpha(), 0.0f);
	EXPECT_LT(stepper.Alpha(), 1.0f);

	Stepper boundary(0.1f, 5u);
	EXPECT_EQ(boundary.Advance(static_cast<double>(boundary.FixedDelta()) * 2.0), 2u);
	[[maybe_unused]] const TickContext boundaryFirst = boundary.NextStep();
	[[maybe_unused]] const TickContext boundarySecond = boundary.NextStep();

	EXPECT_NEAR(boundary.Alpha(), 0.0f, kAlphaEpsilon);
	EXPECT_GE(boundary.Alpha(), 0.0f);
	EXPECT_LT(boundary.Alpha(), 1.0f);
}
