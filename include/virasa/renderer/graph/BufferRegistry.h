#ifndef VIRASA_RENDERER_GRAPH_BUFFER_REGISTRY_H
#define VIRASA_RENDERER_GRAPH_BUFFER_REGISTRY_H

#include <cstdint>
#include <vector>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/graph/Types.h"
#include "virasa/renderer/resources/Buffer.h"

namespace virasa
{
class Device;
}

namespace virasa::renderer::graph
{

/**
 * @brief Owns a dynamically-sized pool of Buffer slots for transient render-graph buffers.
 *
 * BufferRegistry manages a collection of slots, each holding a virasa::renderer::resources::Buffer
 * together with its cached GraphBufferDesc, current ResourceUsage, and an allocated/free flag.
 * Freed slots are retained in a LIFO free-list and may be reclaimed by a subsequent Allocate call
 * whose desc matches the cached desc, avoiding per-frame Vulkan allocation churn and preserving
 * stable VkDeviceAddress values for BDA push-constant payloads.
 *
 * BufferRegistry borrows (does not own) the Device it was initialized against; the Device must
 * outlive any initialized BufferRegistry.
 *
 * BufferRegistry is not thread-safe per instance.
 */
class BufferRegistry final
{
	public:
	/**
	 * @brief Default-constructs an uninitialized, empty BufferRegistry.
	 */
	BufferRegistry() = default;

	~BufferRegistry();

	BufferRegistry(const BufferRegistry&) = delete;
	BufferRegistry& operator=(const BufferRegistry&) = delete;

	BufferRegistry(BufferRegistry&& other) noexcept;
	BufferRegistry& operator=(BufferRegistry&& other) noexcept;

	/**
	 * @brief Initializes the registry against the given Device.
	 *
	 * If already initialized, tears down all existing slots before storing the new Device
	 * pointer.
	 *
	 * @param device A fully-initialized Device that will be used for all subsequent Buffer
	 * creations.
	 * @return RenderError::None on success.
	 */
	[[nodiscard]] virasa::RenderError Initialize(const virasa::Device& device);

	/**
	 * @brief Allocates a slot whose Buffer matches desc, reusing a free slot when possible.
	 *
	 * Scans the free-list LIFO for a slot whose cached GraphBufferDesc is field-equal to desc.
	 * If found, reclaims that slot. Otherwise creates a new Buffer via Buffer::Initialize.
	 * Returns the invalid sentinel BufferHandle (IsValid() == false) on Buffer creation failure.
	 *
	 * @param desc Description of the buffer to allocate.
	 * @return A BufferHandle naming the allocated slot, or the invalid sentinel on failure.
	 */
	[[nodiscard]] BufferHandle Allocate(const GraphBufferDesc& desc);

	/**
	 * @brief Returns the slot named by handle to the free-list without destroying its Buffer.
	 *
	 * @param handle A handle previously returned by a successful Allocate call and not since
	 * Freed.
	 */
	void Free(BufferHandle handle);

	/**
	 * @brief Returns a const reference to the Buffer stored in the slot named by handle.
	 *
	 * @param handle A currently-allocated BufferHandle.
	 * @return Const reference to the underlying Buffer.
	 */
	[[nodiscard]] const virasa::Buffer& Get(BufferHandle handle) const noexcept;

	/**
	 * @brief Returns the cached GraphBufferDesc for the slot named by handle.
	 *
	 * @param handle A currently-allocated BufferHandle.
	 * @return Const reference to the cached GraphBufferDesc.
	 */
	[[nodiscard]] const GraphBufferDesc& GetDesc(BufferHandle handle) const noexcept;

	/**
	 * @brief Returns the cached ResourceUsage for the slot named by handle.
	 *
	 * @param handle A currently-allocated BufferHandle.
	 * @return The current ResourceUsage of the slot.
	 */
	[[nodiscard]] ResourceUsage GetUsage(BufferHandle handle) const noexcept;

	/**
	 * @brief Overwrites the cached ResourceUsage for the slot named by handle.
	 *
	 * @param handle A currently-allocated BufferHandle.
	 * @param usage The new ResourceUsage to cache on the slot.
	 */
	void SetUsage(BufferHandle handle, ResourceUsage usage);

	/**
	 * @brief Returns true if handle names a currently-allocated slot.
	 *
	 * Safe to call with any BufferHandle value, including the invalid sentinel.
	 *
	 * @param handle Any BufferHandle value.
	 * @return true if the slot is allocated, false otherwise.
	 */
	[[nodiscard]] bool IsAllocated(BufferHandle handle) const noexcept;

	/**
	 * @brief Returns the total number of slots ever created by the creation rule of Allocate.
	 * @return Current slot collection size.
	 */
	[[nodiscard]] uint32_t GetSlotCount() const noexcept;

	/**
	 * @brief Returns the number of currently-allocated slots.
	 * @return Number of slots for which IsAllocated would return true.
	 */
	[[nodiscard]] uint32_t GetAllocatedCount() const noexcept;

	/**
	 * @brief Returns true if Initialize has been called and no move-from has since occurred.
	 * @return Initialization state.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

	private:
	struct Slot
	{
		Buffer buffer;
		GraphBufferDesc desc;
		ResourceUsage usage = ResourceUsage::Undefined;
		bool allocated = false;
	};

	void Teardown();

	const Device* _device = nullptr;
	bool _initialized = false;
	std::vector<Slot> _slots;
	std::vector<uint32_t> _freeList;
};

} // namespace virasa::renderer::graph

#endif // VIRASA_RENDERER_GRAPH_BUFFER_REGISTRY_H
