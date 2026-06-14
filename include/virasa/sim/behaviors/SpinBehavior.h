#ifndef VIRASA_SIM_BEHAVIORS_SPINBEHAVIOR_H
#define VIRASA_SIM_BEHAVIORS_SPINBEHAVIOR_H

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
 * @brief Fixed-step behavior that applies SpinComponent angular velocity to local transforms.
 */
class SpinBehavior final : public virasa::ecs::Behavior
{
public:
	SpinBehavior() = default;
	SpinBehavior(const SpinBehavior&) = delete;
	SpinBehavior& operator=(const SpinBehavior&) = delete;
	SpinBehavior(SpinBehavior&&) = delete;
	SpinBehavior& operator=(SpinBehavior&&) = delete;

	/// @brief Virtual destructor.
	virtual ~SpinBehavior() noexcept = default;

	/**
	 * @brief Return the stable behavior name.
	 * @return Null-terminated "Spin" string.
	 */
	[[nodiscard]] const char* Name() const noexcept override;

	/**
	 * @brief Apply one fixed-step spin update to all matching entities.
	 * @param world World containing Spin and Transform component systems.
	 * @param tick Fixed-step timing data.
	 * @param commands Deferred command buffer, left untouched by this behavior.
	 */
	void Step(
		virasa::ecs::World& world,
		const virasa::sim::TickContext& tick,
		virasa::ecs::CommandBuffer& commands) override;
};

} // namespace virasa::sim::behaviors

#endif // VIRASA_SIM_BEHAVIORS_SPINBEHAVIOR_H
