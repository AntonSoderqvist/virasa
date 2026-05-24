#ifndef VIRASA_RENDERER_GRAPH_IMAGE_REGISTRY_H
#define VIRASA_RENDERER_GRAPH_IMAGE_REGISTRY_H

#include <cstdint>
#include <vector>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/graph/Types.h"
#include "virasa/renderer/resources/Image.h"

namespace virasa
{
class Device;
}

namespace virasa::renderer::graph
{

/**
 * @brief Owns a dynamically-sized collection of Image slots for use by the render graph.
 *
 * Each slot holds at most one resources::Image plus cached per-slot metadata:
 * the GraphImageDesc the Image was created from, the current ResourceUsage of the slot, and an
 * allocated/free flag. A free-slot list enables desc-matched reuse across frames without
 * per-frame allocation churn.
 *
 * ImageRegistry borrows (does not own) the Device it was initialized against; the Device must
 * outlive any initialized ImageRegistry.
 */
class ImageRegistry final
{
	public:
	/**
	 * @brief Default-constructs an empty, uninitialized ImageRegistry.
	 *
	 * IsInitialized returns false, GetSlotCount returns 0, GetAllocatedCount returns 0.
	 * No Vulkan API calls or heap allocations are performed.
	 */
	ImageRegistry() = default;

	/// Destructor — destroys all contained Images in slot-index order.
	~ImageRegistry();

	ImageRegistry(const ImageRegistry&) = delete;
	ImageRegistry& operator=(const ImageRegistry&) = delete;

	/// Move constructor — transfers ownership from source; source is left in default-constructed
	/// state.
	ImageRegistry(ImageRegistry&& other) noexcept;

	/// Move assignment — tears down existing slots, then takes ownership from source.
	ImageRegistry& operator=(ImageRegistry&& other) noexcept;

	/**
	 * @brief Initializes the registry against the given device.
	 *
	 * If already initialized, tears down all existing slots before re-initializing.
	 *
	 * @param device A fully initialized Device that will be used for subsequent Allocate calls.
	 * @return RenderError::None on success.
	 */
	[[nodiscard]] virasa::RenderError Initialize(const virasa::Device& device);

	/**
	 * @brief Allocates a slot whose contained Image matches desc.
	 *
	 * Reuses a free slot with a matching desc (LIFO) if available; otherwise creates a new slot.
	 *
	 * @param desc Description of the image to allocate.
	 * @return A valid ImageHandle on success, or an invalid sentinel handle on failure.
	 */
	[[nodiscard]] ImageHandle Allocate(const GraphImageDesc& desc);

	/**
	 * @brief Returns a slot to the free-slot list without destroying its Image.
	 *
	 * @param handle A valid, currently-allocated handle returned by a prior Allocate call.
	 */
	void Free(ImageHandle handle);

	/**
	 * @brief Returns a const reference to the Image in the given slot.
	 *
	 * @param handle A valid, currently-allocated handle.
	 * @return Const reference to the contained Image.
	 */
	[[nodiscard]] const Image& Get(ImageHandle handle) const noexcept;

	/**
	 * @brief Returns a const reference to the GraphImageDesc cached on the given slot.
	 *
	 * @param handle A valid, currently-allocated handle.
	 * @return Const reference to the cached GraphImageDesc.
	 */
	[[nodiscard]] const GraphImageDesc& GetDesc(ImageHandle handle) const noexcept;

	/**
	 * @brief Returns the cached ResourceUsage for the given slot.
	 *
	 * @param handle A valid, currently-allocated handle.
	 * @return The current ResourceUsage of the slot.
	 */
	[[nodiscard]] ResourceUsage GetUsage(ImageHandle handle) const noexcept;

	/**
	 * @brief Overwrites the cached ResourceUsage for the given slot.
	 *
	 * @param handle A valid, currently-allocated handle.
	 * @param usage The new ResourceUsage to store.
	 */
	void SetUsage(ImageHandle handle, ResourceUsage usage);

	/**
	 * @brief Returns true if the given handle refers to a currently-allocated slot.
	 *
	 * @param handle Any ImageHandle value, including the invalid sentinel.
	 * @return true if the slot is allocated, false otherwise.
	 */
	[[nodiscard]] bool IsAllocated(ImageHandle handle) const noexcept;

	/**
	 * @brief Returns the total number of slots ever created (allocated or free).
	 * @return Current slot collection size.
	 */
	[[nodiscard]] uint32_t GetSlotCount() const noexcept;

	/**
	 * @brief Returns the number of currently-allocated slots.
	 * @return Number of allocated slots.
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
		Image image;
		GraphImageDesc desc;
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

#endif // VIRASA_RENDERER_GRAPH_IMAGE_REGISTRY_H
