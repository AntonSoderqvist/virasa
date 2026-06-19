#ifndef VIRASA_SIM_BEHAVIORS_PHYSICSBEHAVIOR_H
#define VIRASA_SIM_BEHAVIORS_PHYSICSBEHAVIOR_H

#include "virasa/ecs/Scheduler.h"

#include <memory>

namespace virasa::ecs
{
class CommandBuffer;
class World;
}

namespace virasa::physics
{
class PhysicsWorld;
}

namespace virasa::sim
{
struct TickContext;
}

namespace virasa::sim::behaviors
{

/**
 * @brief Fixed-step behavior that drives the owned rigid-body physics simulation.
 */
class PhysicsBehavior final : public virasa::ecs::Behavior
{
public:
	/**
	 * @brief Construct an empty physics behavior with a default-configured physics world.
	 */
	PhysicsBehavior();

	PhysicsBehavior(const PhysicsBehavior&) = delete;
	PhysicsBehavior& operator=(const PhysicsBehavior&) = delete;
	PhysicsBehavior(PhysicsBehavior&&) = delete;
	PhysicsBehavior& operator=(PhysicsBehavior&&) = delete;

	/// @brief Virtual destructor.
	virtual ~PhysicsBehavior() noexcept;

	/**
	 * @brief Return the stable behavior name.
	 * @return Null-terminated "Physics" string.
	 */
	[[nodiscard]] const char* Name() const noexcept override;

	/**
	 * @brief Apply one fixed-step physics update and sync simulated transforms.
	 * @param world World containing RigidBody, Collider, and Transform component systems.
	 * @param tick Fixed-step timing data.
	 * @param commands Deferred command buffer, left untouched by this behavior.
	 */
	void Step(
		virasa::ecs::World& world,
		const virasa::sim::TickContext& tick,
		virasa::ecs::CommandBuffer& commands) override;

private:
	std::unique_ptr<virasa::physics::PhysicsWorld> _physicsWorld;
	bool _populated = false;
};

} // namespace virasa::sim::behaviors

#endif // VIRASA_SIM_BEHAVIORS_PHYSICSBEHAVIOR_H
