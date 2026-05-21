#ifndef VIRASA_RENDERER_RESOURCES_BUFFER_H
#define VIRASA_RENDERER_RESOURCES_BUFFER_H

#include <cstddef>
#include <cstdint>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "vulkan/vulkan.h"

namespace virasa
{

/**
 * @brief Describes the parameters used to create a Buffer's VkBuffer and backing memory.
 */
struct BufferConfig
{
	public:
	/// @brief Size of the buffer in bytes. Default is 0 (invalid; caller must set non-zero).
	VkDeviceSize size = 0;

	/// @brief Vulkan buffer usage flags. Default is 0 (invalid; caller must set).
	VkBufferUsageFlags usage = 0;

	/// @brief Memory property flags for backing allocation. Default is
	/// VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT.
	VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
};

/**
 * @brief Owns a VkBuffer and its backing VkDeviceMemory, with optional persistent host mapping.
 *
 * Buffer is a final, RAII, move-only class. It is not copyable. A default-constructed
 * Buffer owns no Vulkan resources and IsInitialized() returns false. After a successful
 * Initialize call, the Buffer owns the VkBuffer and VkDeviceMemory and borrows the VkDevice.
 * The destructor releases all owned resources.
 */
class Buffer final
{
	public:
	/**
	 * @brief Default constructor. Produces a Buffer that owns no Vulkan resources.
	 */
	Buffer() = default;

	/// Copy constructor deleted — Buffer is not copyable.
	Buffer(const Buffer&) = delete;
	/// Copy assignment deleted — Buffer is not copyable.
	Buffer& operator=(const Buffer&) = delete;

	/**
	 * @brief Move constructor. Transfers ownership from source to this Buffer.
	 * @param other The source Buffer; left in a default-constructed state after the move.
	 */
	Buffer(Buffer&& other) noexcept;

	/**
	 * @brief Move assignment. Destroys current resources, then transfers ownership from source.
	 * @param other The source Buffer; left in a default-constructed state after the move.
	 * @return Reference to this Buffer.
	 */
	Buffer& operator=(Buffer&& other) noexcept;

	/**
	 * @brief Destructor. Unmaps any persistent mapping, destroys the VkBuffer, and frees the
	 * VkDeviceMemory.
	 */
	~Buffer() noexcept;

	/**
	 * @brief Creates the VkBuffer, allocates and binds backing VkDeviceMemory.
	 * @param device An initialized Device whose lifetime must exceed this Buffer's.
	 * @param config Parameters describing the buffer size, usage, and memory properties.
	 * @return RenderError::None on success, or a specific error code on failure.
	 */
	[[nodiscard]] RenderError Initialize(const Device& device, const BufferConfig& config);

	/**
	 * @brief Returns a host-accessible pointer to the buffer's backing memory.
	 *
	 * Implements a persistent-mapping cache: if already mapped, returns the cached pointer.
	 * On vkMapMemory failure, logs at Error level and returns nullptr.
	 *
	 * @return Pointer to mapped memory, or nullptr on failure.
	 */
	[[nodiscard]] void* Map();

	/**
	 * @brief Releases the persistent mapping if one exists. No-op if not mapped.
	 */
	void Unmap();

	/**
	 * @brief Writes size_bytes bytes from data into the start of the buffer's host-visible
	 * memory.
	 * @param data Pointer to source data.
	 * @param size_bytes Number of bytes to write.
	 * @return RenderError::None on success, RenderError::MemoryMapFailed on map failure.
	 */
	[[nodiscard]] RenderError Upload(const void* data, VkDeviceSize size_bytes);

	/**
	 * @brief Writes size bytes from data into the buffer's host-visible memory at the given
	 * offset.
	 * @param data Pointer to source data.
	 * @param size Number of bytes to write.
	 * @param offset Byte offset into the buffer.
	 * @return RenderError::None on success, RenderError::MemoryMapFailed on map failure.
	 */
	[[nodiscard]] RenderError Write(const void* data, size_t size, size_t offset);

	/**
	 * @brief Uploads data to a device-local buffer via a temporary staging buffer.
	 *
	 * Allocates a host-visible staging buffer, copies data into it, records and submits
	 * a one-shot transfer command buffer, waits for completion, and frees the command buffer
	 * and staging buffer.
	 *
	 * @param device An initialized Device.
	 * @param transfer_pool VkCommandPool on the transfer queue family.
	 * @param transfer_queue VkQueue for transfer operations.
	 * @param data Pointer to source data.
	 * @param size Number of bytes to upload.
	 * @return RenderError::None on success, or a specific error code on failure.
	 */
	[[nodiscard]] RenderError UploadViaStaging(const Device& device, VkCommandPool transfer_pool,
		VkQueue transfer_queue, const void* data, size_t size);

	/**
	 * @brief Returns the owned VkBuffer handle, or VK_NULL_HANDLE if not initialized.
	 * @return The VkBuffer handle.
	 */
	[[nodiscard]] VkBuffer GetHandle() const noexcept;

	/**
	 * @brief Returns the cached buffer size in bytes.
	 * @return The cached size.
	 */
	[[nodiscard]] VkDeviceSize GetSize() const noexcept;

	/**
	 * @brief Returns true if this Buffer currently owns a valid VkBuffer.
	 * @return True if initialized, false otherwise.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

	private:
	/// Performs teardown: unmaps, destroys buffer, frees memory, resets handles.
	void Teardown() noexcept;

	/// Selects a memory type index satisfying typeFilter and required property flags.
	/// Returns UINT32_MAX on failure.
	uint32_t FindMemoryType(const VkPhysicalDeviceMemoryProperties& memProps, uint32_t typeFilter,
		VkMemoryPropertyFlags required) const noexcept;

	VkDevice _device = VK_NULL_HANDLE;
	VkBuffer _buffer = VK_NULL_HANDLE;
	VkDeviceMemory _memory = VK_NULL_HANDLE;
	VkDeviceSize _size = 0;
	void* _mapped = nullptr;
};

} // namespace virasa

#endif // VIRASA_RENDERER_RESOURCES_BUFFER_H
