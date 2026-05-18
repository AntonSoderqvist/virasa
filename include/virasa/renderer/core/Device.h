#ifndef VIRASA_RENDERER_CORE_DEVICE_H
#define VIRASA_RENDERER_CORE_DEVICE_H

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Instance.h"

#include "vulkan/vulkan.h"

namespace virasa
{

/**
 * @brief Owns and manages the Vulkan logical device (VkDevice) and its associated queues.
 *
 * Device selects a suitable VkPhysicalDevice, creates a VkDevice against it, and
 * retrieves the graphics, present, and transfer VkQueue handles. It is final and
 * non-copyable. It is default-constructible and movable.
 */
class Device final
{
public:
	/**
	 * @brief Default-constructs a Device that owns no Vulkan handle.
	 */
	Device() = default;

	/// Non-copyable.
	Device(const Device&) = delete;
	Device& operator=(const Device&) = delete;

	/**
	 * @brief Move-constructs a Device, transferring ownership from the source.
	 * @param other The source Device; left in a default-constructed state after the move.
	 */
	Device(Device&& other) noexcept;

	/**
	 * @brief Move-assigns a Device, destroying any currently owned handle first.
	 * @param other The source Device; left in a default-constructed state after the move.
	 * @return Reference to this Device.
	 */
	Device& operator=(Device&& other) noexcept;

	/**
	 * @brief Destroys the owned VkDevice, if any.
	 */
	~Device();

	/**
	 * @brief Selects a VkPhysicalDevice and creates a VkDevice against it.
	 *
	 * @param instance An initialized Instance whose VkInstance backs the Vulkan session.
	 * @param surface  A valid VkSurfaceKHR used to check presentation support.
	 * @return RenderError::None on success, RenderError::NoSuitableDevice if no physical
	 *         device meets the hard requirements, or RenderError::DeviceCreateFailed if
	 *         vkCreateDevice fails.
	 */
	[[nodiscard]] RenderError Initialize(const Instance& instance, VkSurfaceKHR surface);

	/**
	 * @brief Returns the owned VkDevice handle, or VK_NULL_HANDLE if none is owned.
	 * @return The VkDevice handle.
	 */
	[[nodiscard]] VkDevice GetHandle() const noexcept;

	/**
	 * @brief Returns the selected VkPhysicalDevice handle, or VK_NULL_HANDLE if not initialized.
	 * @return The VkPhysicalDevice handle.
	 */
	[[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const noexcept;

	/**
	 * @brief Returns the graphics VkQueue handle, or VK_NULL_HANDLE if not initialized.
	 * @return The graphics queue.
	 */
	[[nodiscard]] VkQueue GetGraphicsQueue() const noexcept;

	/**
	 * @brief Returns the present VkQueue handle, or VK_NULL_HANDLE if not initialized.
	 * @return The present queue.
	 */
	[[nodiscard]] VkQueue GetPresentQueue() const noexcept;

	/**
	 * @brief Returns the transfer VkQueue handle, or VK_NULL_HANDLE if not initialized.
	 *
	 * If no dedicated transfer queue family was found, returns the same handle as
	 * GetGraphicsQueue().
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
	 * @return The graphicsFamily field of the cached QueueFamilies.
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
	 * @brief Returns true if this Device owns a valid VkDevice handle.
	 * @return True if initialized, false otherwise.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

	/**
	 * @brief Blocks until all queues on the owned VkDevice have completed all submitted work.
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
	QueueFamilies _queueFamilies{};
	VkPhysicalDeviceProperties _properties{};
	VkPhysicalDeviceMemoryProperties _memoryProperties{};
};

} // namespace virasa

#endif // VIRASA_RENDERER_CORE_DEVICE_H
