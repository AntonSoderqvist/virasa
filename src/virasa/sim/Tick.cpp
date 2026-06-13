#include "virasa/sim/Tick.h"

namespace virasa::sim
{

double Clock::Sample() noexcept
{
	const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	const std::chrono::duration<double> elapsed = now - _instant;
	_instant = now;

	const double seconds = elapsed.count();
	if (seconds < 0.0)
	{
		return 0.0;
	}

	return seconds;
}

void Clock::Reset() noexcept
{
	_instant = std::chrono::steady_clock::now();
}

Stepper::Stepper(float fixedDeltaSeconds, uint32_t maxStepsPerFrame)
    : _fixedDelta(fixedDeltaSeconds), _maxStepsPerFrame(maxStepsPerFrame)
{
}

uint32_t Stepper::Advance(double rawDeltaSeconds)
{
	if (_paused)
	{
		return 0u;
	}

	_accumulator += rawDeltaSeconds * static_cast<double>(_timeScale);

	const double fixedDelta = static_cast<double>(_fixedDelta);
	const auto wholeSteps = static_cast<uint64_t>(_accumulator / fixedDelta);
	if (wholeSteps > _maxStepsPerFrame)
	{
		const double remainder = _accumulator - (static_cast<double>(wholeSteps) * fixedDelta);
		_accumulator = (static_cast<double>(_maxStepsPerFrame) * fixedDelta) + remainder;
		return _maxStepsPerFrame;
	}

	return static_cast<uint32_t>(wholeSteps);
}

virasa::sim::TickContext Stepper::NextStep()
{
	_accumulator -= static_cast<double>(_fixedDelta);
	++_tickIndex;
	_totalTime += static_cast<double>(_fixedDelta);

	return virasa::sim::TickContext{
		.deltaTime = _fixedDelta,
		.totalTime = _totalTime,
		.tickIndex = _tickIndex,
		.alpha = Alpha(),
		.timeScale = _timeScale,
		.paused = _paused,
	};
}

float Stepper::Alpha() const noexcept
{
	return static_cast<float>(_accumulator / static_cast<double>(_fixedDelta));
}

float Stepper::FixedDelta() const noexcept
{
	return _fixedDelta;
}

uint64_t Stepper::TickIndex() const noexcept
{
	return _tickIndex;
}

double Stepper::TotalTime() const noexcept
{
	return _totalTime;
}

void Stepper::SetTimeScale(float timeScale)
{
	_timeScale = timeScale;
}

float Stepper::GetTimeScale() const noexcept
{
	return _timeScale;
}

void Stepper::SetPaused(bool paused)
{
	_paused = paused;
}

bool Stepper::IsPaused() const noexcept
{
	return _paused;
}

} // namespace virasa::sim
