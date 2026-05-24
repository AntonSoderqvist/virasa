#ifndef VIRASA_RENDERER_CORE_DEVICE_H
#define VIRASA_RENDERER_CORE_DEVICE_H

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Instance.h"

#include "vulkan/vulkan.h"

#include <cstdint>

namespace virasa
{

/**
 * @brief Selects a VkPhysicalDevice and owns the resulting VkDevice.
 *
 * Device is a RAII wrapper around a Vulkan logical device. It is responsible
 * for physical-device selection, logical-device creation, and retrieval of
 * queue handles. Device is final and non-copyable; it is movable.
 */
class Device final
{
public:
	/**
	 * @brief Default-constructs a Device that owns no Vulkan handle.
	 */
	Device() = default;

	/**
	 * @brief Destroys the owned VkDevice, if any.
	 */
	~Device();

	// Non-copyable
	Device(const Device&) = delete;
	Device& operator=(const Device&) = delete;

	// Movable
	Device(Device&& other) noexcept;
	Device& operator=(Device&& other) noexcept;

	/**
	 * @brief Selects a physical device and creates a logical VkDevice.
	 * @param instance An initialized Instance whose VkInstance backs the surface.
	 * @param surface A valid VkSurfaceKHR created from the same VkInstance.
	 * @return RenderError::None on success, or a specific error code on failure.
	 */
	[[nodiscard]] RenderError Initialize(const Instance& instance, VkSurfaceKHR surface);

	/**
	 * @brief Returns the owned VkDevice handle, or VK_NULL_HANDLE if none.
	 * @return The owned VkDevice.
	 */
	[[nodiscard]] VkDevice GetHandle() const noexcept;

	/**
	 * @brief Returns the selected VkPhysicalDevice, or VK_NULL_HANDLE if none.
	 * @return The selected VkPhysicalDevice.
	 */
	[[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const noexcept;

	/**
	 * @brief Returns the graphics VkQueue handle.
	 * @return The graphics queue.
	 */
	[[nodiscard]] VkQueue GetGraphicsQueue() const noexcept;

	/**
	 * @brief Returns the present VkQueue handle.
	 * @return The present queue.
	 */
	[[nodiscard]] VkQueue GetPresentQueue() const noexcept;

	/**
	 * @brief Returns the transfer VkQueue handle (dedicated or graphics fallback).
	 * @return The transfer queue.
	 */
	[[nodiscard]] VkQueue GetTransferQueue() const noexcept;

	/**
	 * @brief Returns a reference to the cached QueueFamilies struct.
	 * @return Const reference to the QueueFamilies.
	 */
	[[nodiscard]] const QueueFamilies& GetQueueFamilies() const noexcept;

	/**
	 * @brief Returns the graphics queue family index.
	 * @return The graphics family index.
	 */
	[[nodiscard]] uint32_t GetGraphicsQueueFamilyIndex() const noexcept;

	/**
	 * @brief Returns a reference to the cached VkPhysicalDeviceMemoryProperties.
	 * @return Const reference to the memory properties.
	 */
	[[nodiscard]] const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const noexcept;

	/**
	 * @brief Returns a reference to the cached VkPhysicalDeviceProperties.
	 * @return Const reference to the device properties.
	 */
	[[nodiscard]] const VkPhysicalDeviceProperties& GetProperties() const noexcept;

	/**
	 * @brief Returns true if the Device owns a valid VkDevice handle.
	 * @return True if initialized.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

	/**
	 * @brief Returns the VkDeviceAddress for the given buffer.
	 * @param buffer A VkBuffer created with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT.
	 * @return The VkDeviceAddress of the buffer.
	 */
	[[nodiscard]] VkDeviceAddress GetBufferDeviceAddress(VkBuffer buffer) const noexcept;

	/**
	 * @brief Returns true if buffer device address is enabled on this device.
	 * @return True if bufferDeviceAddress is enabled.
	 */
	[[nodiscard]] bool IsBufferDeviceAddressEnabled() const noexcept;

	/**
	 * @brief Returns true if descriptor indexing features are enabled on this device.
	 * @return True if descriptor indexing is enabled.
	 */
	[[nodiscard]] bool IsDescriptorIndexingEnabled() const noexcept;

	/**
	 * @brief Blocks until all queues on the owned VkDevice have completed work.
	 *
	 * No-op if the Device is not initialized.
	 */
	void WaitIdle() const;

private:
	VkDevice _device = VK_NULL_HANDLE;
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
	VkQueue _graphicsQueue = VK_NULL_HANDLE;
	VkQueue _presentQueue = VK_NULL_HANDLE;
	VkQueue _transferQueue = VK_NULL_HANDLE;
	QueueFamilies _queueFamilies = {};
	VkPhysicalDeviceProperties _deviceProperties = {};
	VkPhysicalDeviceMemoryProperties _memoryProperties = {};
};

} // namespace virasa

#endif // VIRASA_RENDERER_CORE_DEVICE_H
