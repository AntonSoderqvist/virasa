#ifndef VIRASA_SIM_BEHAVIORS_CHASECAMERABEHAVIOR_H
#define VIRASA_SIM_BEHAVIORS_CHASECAMERABEHAVIOR_H

#include "virasa/ecs/Scheduler.h"

namespace virasa::ecs
{
class CommandBuffer;
class World;
}

namespace virasa::sim
{
struct TickContext;
}

namespace virasa::sim::behaviors
{

/**
 * @brief Authored follow parameters for the built-in chase camera behavior.
 */
struct ChaseCameraComponent
{
public:
	/// @brief Distance in metres behind the followed target.
	float distance = 8.0f;

	/// @brief Height in metres above the followed target.
	float height = 3.0f;

	/// @brief Height in metres above the target origin to aim at.
	float lookAtHeight = 1.0f;

	/// @brief Exponential position smoothing rate per second.
	float positionSmoothing = 8.0f;
};

/**
 * @brief Fixed-step behavior that positions chase cameras behind the first vehicle.
 */
class ChaseCameraBehavior final : public virasa::ecs::Behavior
{
public:
	ChaseCameraBehavior() = default;
	ChaseCameraBehavior(const ChaseCameraBehavior&) = delete;
	ChaseCameraBehavior& operator=(const ChaseCameraBehavior&) = delete;
	ChaseCameraBehavior(ChaseCameraBehavior&&) = delete;
	ChaseCameraBehavior& operator=(ChaseCameraBehavior&&) = delete;

	/// @brief Virtual destructor.
	virtual ~ChaseCameraBehavior() noexcept = default;

	/**
	 * @brief Return the stable behavior name.
	 * @return Null-terminated "ChaseCamera" string.
	 */
	[[nodiscard]] const char* Name() const noexcept override;

	/**
	 * @brief Apply one fixed-step chase camera update to all matching entities.
	 * @param world World containing ChaseCamera, Vehicle, and Transform component systems.
	 * @param tick Fixed-step timing data.
	 * @param commands Deferred command buffer, left untouched by this behavior.
	 */
	void Step(
		virasa::ecs::World& world,
		const virasa::sim::TickContext& tick,
		virasa::ecs::CommandBuffer& commands) override;
};

} // namespace virasa::sim::behaviors

#endif // VIRASA_SIM_BEHAVIORS_CHASECAMERABEHAVIOR_H
