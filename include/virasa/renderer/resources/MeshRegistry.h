#ifndef VIRASA_RENDERER_RESOURCES_MESHREGISTRY_H
#define VIRASA_RENDERER_RESOURCES_MESHREGISTRY_H

#include <cstdint>
#include <optional>
#include <vector>

#include "virasa/renderer/resources/Mesh.h"

namespace virasa::renderer
{

/**
 * @brief Owns a growable collection of Mesh slots with free-list reuse.
 *
 * MeshRegistry manages a slot-based collection of Mesh objects. Slots are
 * allocated via Allocate (which takes ownership of a Mesh by move) and
 * returned to a free list via Free. The registry is the sole owner of all
 * contained Mesh objects and releases their Vulkan resources on destruction
 * or move-assignment teardown.
 *
 * MeshRegistry is final, default-constructible, movable, and not copyable.
 * It owns no Vulkan handles of its own; the contained Mesh objects borrow
 * their respective Device and Context handles.
 */
class MeshRegistry final
{
	public:
	/// @brief Default-constructs an empty registry with no slots and no free list.
	MeshRegistry() = default;

	MeshRegistry(const MeshRegistry&) = delete;
	MeshRegistry& operator=(const MeshRegistry&) = delete;

	MeshRegistry(MeshRegistry&& other) noexcept;
	MeshRegistry& operator=(MeshRegistry&& other) noexcept;

	~MeshRegistry() = default;

	/**
	 * @brief Takes ownership of a Mesh and returns the slot id.
	 *
	 * If the free list is non-empty the last freed slot is reused; otherwise
	 * the slot collection grows by one. Returns 0xFFFFFFFFu as a sentinel
	 * error value if the slot count has already reached 0xFFFFFFFEu and the
	 * free list is empty.
	 *
	 * @param mesh  Mesh rvalue to move into the registry.
	 * @return Slot id on success, or 0xFFFFFFFFu on failure.
	 */
	[[nodiscard]] uint32_t Allocate(Mesh&& mesh);

	/**
	 * @brief Destroys the Mesh in the given slot and returns the slot to the free list.
	 *
	 * @param id  Slot id previously returned by a successful Allocate call.
	 */
	void Free(uint32_t id);

	/**
	 * @brief Returns a const reference to the Mesh in the given slot.
	 *
	 * @param id  Slot id that is currently allocated.
	 * @return Const reference to the owned Mesh.
	 */
	[[nodiscard]] const Mesh& Get(uint32_t id) const noexcept;

	/**
	 * @brief Returns true if the slot at id is currently allocated.
	 *
	 * Safe to call with any uint32_t value including 0xFFFFFFFFu.
	 *
	 * @param id  Slot id to query.
	 * @return true if allocated, false otherwise.
	 */
	[[nodiscard]] bool IsAllocated(uint32_t id) const noexcept;

	/**
	 * @brief Returns the total number of slots (allocated + freed).
	 * @return Current slot collection size.
	 */
	[[nodiscard]] uint32_t GetSlotCount() const noexcept;

	/**
	 * @brief Returns the number of currently allocated slots.
	 * @return GetSlotCount() minus the free-list size.
	 */
	[[nodiscard]] uint32_t GetAllocatedCount() const noexcept;

	private:
	std::vector<std::optional<Mesh>> _slots;
	std::vector<uint32_t> _freeList;
};

} // namespace virasa::renderer

#endif // VIRASA_RENDERER_RESOURCES_MESHREGISTRY_H
