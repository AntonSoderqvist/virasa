#ifndef VIRASA_PHYSICS_COLLISIONMESHREGISTRY_H
#define VIRASA_PHYSICS_COLLISIONMESHREGISTRY_H

#include "virasa/math/Types.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

namespace virasa::physics
{

/**
 * @brief Authored triangle mesh collision data.
 */
struct CollisionMeshData
{
public:
	std::vector<virasa::math::Vec3> vertices;
	std::vector<uint32_t> indices;
};

/**
 * @brief Registry for collision mesh data addressed by dense integer ids.
 *
 * CollisionMeshRegistry owns registered mesh data for its lifetime. Mesh ids are
 * assigned densely from zero and are never removed.
 */
class CollisionMeshRegistry final
{
public:
	/**
	 * @brief Registers a new collision mesh and returns its assigned id.
	 */
	uint32_t Register(std::vector<virasa::math::Vec3> vertices, std::vector<uint32_t> indices)
	{
		const uint32_t id = static_cast<uint32_t>(entries_.size());
		entries_.push_back(CollisionMeshData{std::move(vertices), std::move(indices)});
		return id;
	}

	/**
	 * @brief Returns true when id names a registered collision mesh.
	 */
	bool Has(uint32_t id) const noexcept
	{
		return id < entries_.size();
	}

	/**
	 * @brief Returns collision mesh data for id, or nullptr when absent.
	 */
	const CollisionMeshData* Get(uint32_t id) const noexcept
	{
		if (!Has(id))
		{
			return nullptr;
		}

		return &entries_[id];
	}

	/**
	 * @brief Returns the number of registered collision meshes.
	 */
	size_t Count() const noexcept
	{
		return entries_.size();
	}

private:
	std::deque<CollisionMeshData> entries_;
};

} // namespace virasa::physics

#endif // VIRASA_PHYSICS_COLLISIONMESHREGISTRY_H
