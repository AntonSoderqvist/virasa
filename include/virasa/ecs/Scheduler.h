#ifndef VIRASA_ECS_SCHEDULER_H
#define VIRASA_ECS_SCHEDULER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Types.h"
#include "virasa/sim/Tick.h"

namespace virasa::ecs
{

class World;

/**
 * @brief Ordered simulation phase executed by Scheduler.
 */
enum class Phase : uint8_t
{
	PreStep = 0,
	Step,
	PostStep
};

/**
 * @brief Records deferred ECS structural changes for a later flush.
 */
class CommandBuffer final
{
	public:
	CommandBuffer() = default;
	~CommandBuffer() = default;

	CommandBuffer(const CommandBuffer&) = delete;
	CommandBuffer& operator=(const CommandBuffer&) = delete;

	CommandBuffer(CommandBuffer&&) noexcept = default;
	CommandBuffer& operator=(CommandBuffer&&) noexcept = default;

	/**
	 * @brief Records creation of an entity with the given name.
	 * @param name The requested entity name, copied immediately.
	 * @return Spawn ticket identifying this pending creation.
	 */
	[[nodiscard]] uint32_t CreateEntity(std::string_view name);

	/**
	 * @brief Records adding raw component bytes to a pending spawn.
	 * @param spawnTicket Ticket returned by CreateEntity().
	 * @param componentId Target component system id.
	 * @param value Pointer to bytes copied immediately.
	 * @param byteSize Number of bytes to copy from value.
	 */
	void AddComponentToSpawnRaw(uint32_t spawnTicket, virasa::ecs::ComponentId componentId,
		const void* value, size_t byteSize);

	/**
	 * @brief Records destruction of an entity.
	 * @param entity Entity to destroy at flush time.
	 */
	void DestroyEntity(virasa::ecs::Entity entity);

	/**
	 * @brief Records adding raw component bytes to an existing entity.
	 * @param entity Entity to receive the component at flush time.
	 * @param componentId Target component system id.
	 * @param value Pointer to bytes copied immediately.
	 * @param byteSize Number of bytes to copy from value.
	 */
	void AddComponentRaw(virasa::ecs::Entity entity, virasa::ecs::ComponentId componentId,
		const void* value, size_t byteSize);

	/**
	 * @brief Records removal of a component from an entity.
	 * @param entity Entity to remove from.
	 * @param componentId Target component system id.
	 */
	void RemoveComponent(virasa::ecs::Entity entity, virasa::ecs::ComponentId componentId);

	/**
	 * @brief Applies all queued commands to world in Scheduler-defined order.
	 * @param world World to mutate.
	 */
	void Flush(virasa::ecs::World& world);

	/**
	 * @brief Discards all queued commands without applying them.
	 */
	void Clear();

	/**
	 * @brief Returns whether there are no queued structural changes.
	 * @return true if all queues are empty.
	 */
	[[nodiscard]] bool IsEmpty() const noexcept;

	private:
	struct PendingCreation
	{
		std::string name;
	};

	struct ComponentAdd
	{
		bool targetsSpawn = false;
		uint32_t spawnTicket = 0u;
		virasa::ecs::Entity entity = virasa::ecs::Entity::Invalid();
		virasa::ecs::ComponentId componentId = virasa::ecs::kInvalidComponentId;
		std::vector<uint8_t> bytes;
	};

	struct ComponentRemoval
	{
		virasa::ecs::Entity entity = virasa::ecs::Entity::Invalid();
		virasa::ecs::ComponentId componentId = virasa::ecs::kInvalidComponentId;
	};

	std::vector<PendingCreation> _creations;
	std::vector<ComponentAdd> _componentAdds;
	std::vector<ComponentRemoval> _componentRemovals;
	std::vector<virasa::ecs::Entity> _destructions;
};

/**
 * @brief Abstract per-tick behavior owned and executed by Scheduler.
 */
class Behavior
{
	public:
	Behavior() = default;
	Behavior(const Behavior&) = delete;
	Behavior& operator=(const Behavior&) = delete;
	Behavior(Behavior&&) = delete;
	Behavior& operator=(Behavior&&) = delete;

	/// @brief Virtual destructor so derived behaviors are properly destroyed through base
	/// pointer.
	virtual ~Behavior() noexcept = default;

	/**
	 * @brief Returns the stable human-readable behavior name.
	 * @return A const char* that outlives this behavior.
	 */
	[[nodiscard]] virtual const char* Name() const noexcept = 0;

	/**
	 * @brief Executes one tick of behavior logic, recording structural changes into commands.
	 * @param world World available for reads and direct non-structural edits.
	 * @param tick Per-step timing context.
	 * @param commands Deferred structural command buffer.
	 */
	virtual void Step(virasa::ecs::World& world, const virasa::sim::TickContext& tick,
		virasa::ecs::CommandBuffer& commands) = 0;
};

/**
 * @brief Runs registered behaviors by phase and owns deferred command flushing.
 */
class Scheduler final
{
	public:
	Scheduler() = default;
	~Scheduler() = default;

	Scheduler(const Scheduler&) = delete;
	Scheduler& operator=(const Scheduler&) = delete;

	Scheduler(Scheduler&&) noexcept = default;
	Scheduler& operator=(Scheduler&&) noexcept = default;

	/**
	 * @brief Registers a behavior at the end of a phase's execution list.
	 * @param phase Phase to append to.
	 * @param behavior Behavior instance to own.
	 */
	void Register(virasa::ecs::Phase phase, std::unique_ptr<virasa::ecs::Behavior> behavior);

	/**
	 * @brief Executes all phases for one tick, flushing commands after each phase.
	 * @param world World to step.
	 * @param tick Per-step timing context.
	 */
	void Step(virasa::ecs::World& world, const virasa::sim::TickContext& tick);

	/**
	 * @brief Enables or disables stepping.
	 * @param enabled True to execute future Step calls.
	 */
	void SetEnabled(bool enabled);

	/**
	 * @brief Returns whether stepping is enabled.
	 * @return true when enabled.
	 */
	[[nodiscard]] bool IsEnabled() const noexcept;

	/**
	 * @brief Returns the number of behaviors registered for a phase.
	 * @param phase Phase to inspect.
	 * @return Number of behaviors in that phase.
	 */
	[[nodiscard]] size_t BehaviorCount(virasa::ecs::Phase phase) const noexcept;

	private:
	static constexpr size_t kPhaseCount = 3u;

	[[nodiscard]] static size_t PhaseIndex(virasa::ecs::Phase phase) noexcept;

	std::array<std::vector<std::unique_ptr<virasa::ecs::Behavior>>, kPhaseCount> _behaviors;
	virasa::ecs::CommandBuffer _commands;
	bool _enabled = true;
};

} // namespace virasa::ecs

#endif // VIRASA_ECS_SCHEDULER_H
