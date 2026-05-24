#ifndef VIRASA_ECS_TYPES_H
#define VIRASA_ECS_TYPES_H

#include <cstdint>

namespace virasa::ecs
{

/**
 * @brief A generational handle to an entity stored in a World.
 *
 * Entity is a lightweight 64-bit value consisting of an index and a generation.
 * It is copyable, movable, default-constructible, and trivially destructible.
 */
struct Entity
{
	public:
	/**
	 * @brief Slot index in the entity table.
	 *
	 * Defaults to 0xFFFFFFFFu, the sentinel for an invalid entity.
	 */
	uint32_t index = 0xFFFFFFFFu;

	/**
	 * @brief Generation of the slot at the time the handle was issued.
	 *
	 * Defaults to 0.
	 */
	uint32_t generation = 0u;

	/**
	 * @brief Equality comparison.
	 * @param other The other Entity to compare.
	 * @return true if both index and generation are bitwise equal.
	 */
	bool operator==(const Entity& other) const noexcept
	{
		return index == other.index && generation == other.generation;
	}

	/**
	 * @brief Inequality comparison.
	 * @param other The other Entity to compare.
	 * @return true if either index or generation differ.
	 */
	bool operator!=(const Entity& other) const noexcept
	{
		return !(*this == other);
	}

	/**
	 * @brief Checks whether this handle is valid locally.
	 *
	 * A handle is valid if its index is not the sentinel value 0xFFFFFFFFu.
	 * This does not check whether the entity is alive in any World.
	 *
	 * @return true if index != 0xFFFFFFFFu.
	 */
	[[nodiscard]] bool IsValid() const noexcept
	{
		return index != 0xFFFFFFFFu;
	}

	/**
	 * @brief Returns the canonical invalid Entity sentinel.
	 *
	 * The returned Entity has index 0xFFFFFFFFu and generation 0,
	 * matching a default-constructed Entity.
	 *
	 * @return An invalid Entity.
	 */
	[[nodiscard]] static Entity Invalid() noexcept
	{
		Entity e;
		e.index = 0xFFFFFFFFu;
		e.generation = 0u;
		return e;
	}
};

/**
 * @brief Error codes for ECS operations.
 *
 * EcsError is a closed set of error values for ECS operations.
 * None indicates success; other values indicate specific failure modes.
 */
enum class EcsError : uint8_t
{
	None = 0,		 ///< Operation succeeded.
	InvalidEntity = 1, ///< The Entity argument did not refer to a live entity.
	CycleDetected = 2, ///< The parenting operation would create a cycle.
	SelfParent =
		3 ///< The parenting operation was rejected because parent and child are the same.
};

} // namespace virasa::ecs

#endif // VIRASA_ECS_TYPES_H
