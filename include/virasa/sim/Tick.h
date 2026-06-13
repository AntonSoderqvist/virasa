#ifndef VIRASA_SIM_TICK_H
#define VIRASA_SIM_TICK_H

#include <chrono>
#include <cstdint>

namespace virasa::sim
{

/**
 * @brief Per-step timing data passed to fixed-rate simulation systems.
 *
 * TickContext describes one committed simulation step and the interpolation
 * alpha remaining in the owning Stepper accumulator after that step.
 */
struct TickContext
{
	public:
	/// @brief Fixed simulation step duration in seconds.
	float deltaTime = 0.0f;
	/// @brief Total simulated time after this step, in seconds.
	double totalTime = 0.0;
	/// @brief Monotonic simulation tick index after this step.
	uint64_t tickIndex = 0u;
	/// @brief Fractional accumulator remainder divided by the fixed delta.
	float alpha = 0.0f;
	/// @brief Scale applied to raw frame deltas before accumulation.
	float timeScale = 1.0f;
	/// @brief Whether the simulation stepper is currently paused.
	bool paused = false;
};

/**
 * @brief Monotonic wall-clock sampler backed by std::chrono::steady_clock.
 *
 * Clock stores one reference instant. Sampling returns elapsed seconds since
 * that instant and advances the instant to the current steady-clock time.
 */
class Clock final
{
	public:
	/**
	 * @brief Return elapsed seconds since the previous sample or reset.
	 * @return Non-negative elapsed wall-clock time in seconds.
	 */
	[[nodiscard]] double Sample() noexcept;

	/**
	 * @brief Reset the reference instant to the current steady-clock time.
	 */
	void Reset() noexcept;

	private:
	std::chrono::steady_clock::time_point _instant = std::chrono::steady_clock::now();
};

/**
 * @brief Fixed-timestep accumulator for deterministic simulation stepping.
 *
 * Stepper accumulates scaled raw frame time, reports how many fixed steps are
 * available, and commits individual steps through NextStep().
 */
class Stepper final
{
	public:
	/**
	 * @brief Construct a Stepper with a fixed simulation delta and frame step cap.
	 * @param fixedDeltaSeconds Fixed simulation step duration in seconds.
	 * @param maxStepsPerFrame Maximum steps to report from one Advance() call.
	 */
	explicit Stepper(float fixedDeltaSeconds, uint32_t maxStepsPerFrame);

	/**
	 * @brief Accumulate scaled raw frame time and return available fixed steps.
	 * @param rawDeltaSeconds Raw frame delta in seconds.
	 * @return Number of fixed steps available, capped by maxStepsPerFrame.
	 */
	[[nodiscard]] uint32_t Advance(double rawDeltaSeconds);

	/**
	 * @brief Commit and describe the next fixed simulation step.
	 * @return TickContext for the committed step.
	 */
	[[nodiscard]] virasa::sim::TickContext NextStep();

	/**
	 * @brief Return interpolation alpha from the current accumulator remainder.
	 * @return Fractional accumulator remainder divided by the fixed delta.
	 */
	[[nodiscard]] float Alpha() const noexcept;

	/**
	 * @brief Get the fixed simulation step duration.
	 * @return Fixed simulation step duration in seconds.
	 */
	[[nodiscard]] float FixedDelta() const noexcept;

	/**
	 * @brief Get the current committed simulation tick index.
	 * @return Current simulation tick index.
	 */
	[[nodiscard]] uint64_t TickIndex() const noexcept;

	/**
	 * @brief Get the current committed total simulation time.
	 * @return Current total simulation time in seconds.
	 */
	[[nodiscard]] double TotalTime() const noexcept;

	/**
	 * @brief Set the scale applied to raw frame deltas during Advance().
	 * @param timeScale Scale factor for future raw frame deltas.
	 */
	void SetTimeScale(float timeScale);

	/**
	 * @brief Get the scale applied to raw frame deltas during Advance().
	 * @return Current time scale.
	 */
	[[nodiscard]] float GetTimeScale() const noexcept;

	/**
	 * @brief Set whether Advance() should ignore raw frame deltas.
	 * @param paused True to pause accumulation, false to resume.
	 */
	void SetPaused(bool paused);

	/**
	 * @brief Get whether the Stepper is currently paused.
	 * @return True when paused.
	 */
	[[nodiscard]] bool IsPaused() const noexcept;

	private:
	double _accumulator = 0.0;
	float _fixedDelta = 0.0f;
	uint32_t _maxStepsPerFrame = 0u;
	uint64_t _tickIndex = 0u;
	double _totalTime = 0.0;
	float _timeScale = 1.0f;
	bool _paused = false;
};

} // namespace virasa::sim

#endif // VIRASA_SIM_TICK_H
