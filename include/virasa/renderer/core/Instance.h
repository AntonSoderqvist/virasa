#ifndef VIRASA_RENDERER_CORE_INSTANCE_H
#define VIRASA_RENDERER_CORE_INSTANCE_H

#include "virasa/renderer/Types.h"

#include <vulkan/vulkan.h>

namespace virasa
{

/**
 * @brief Owns and manages the lifecycle of a VkInstance and optional VkDebugUtilsMessengerEXT.
 *
 * Instance is a RAII handle owner for a Vulkan instance. It is default-constructible
 * (owning no handles), movable (transferring ownership), and not copyable. It is final
 * and cannot be subclassed.
 */
class Instance final
{
public:
	/**
	 * @brief Default-constructs an Instance that owns no Vulkan handles.
	 *
	 * No Vulkan API calls are made. GetHandle returns VK_NULL_HANDLE,
	 * IsInitialized returns false, and IsValidationEnabled returns false.
	 */
	Instance() noexcept = default;

	/// Non-copyable.
	Instance(const Instance&) = delete;
	Instance& operator=(const Instance&) = delete;

	/**
	 * @brief Move-constructs an Instance, transferring ownership of Vulkan handles.
	 * @param other The source Instance; left in the default-constructed state after the move.
	 */
	Instance(Instance&& other) noexcept;

	/**
	 * @brief Move-assigns an Instance, destroying any currently owned handles first.
	 * @param other The source Instance; left in the default-constructed state after the move.
	 * @return Reference to this Instance.
	 */
	Instance& operator=(Instance&& other) noexcept;

	/**
	 * @brief Destroys any owned VkInstance and VkDebugUtilsMessengerEXT handles.
	 */
	~Instance();

	/**
	 * @brief Creates a Vulkan 1.3 instance from the supplied configuration.
	 *
	 * If the Instance is already initialized, returns RenderError::AlreadyInitialized.
	 * Validates that all required layers and extensions are available before calling
	 * vkCreateInstance. If validation is enabled, also creates a VkDebugUtilsMessengerEXT
	 * that forwards messages to the virasa.core.Logger system.
	 *
	 * @param config The renderer configuration describing the desired instance.
	 * @return RenderError::None on success, or a specific error code on failure.
	 */
	[[nodiscard]] RenderError Initialize(const RendererConfig& config);

	/**
	 * @brief Returns the owned VkInstance handle, or VK_NULL_HANDLE if none is owned.
	 * @return The VkInstance handle.
	 */
	[[nodiscard]] VkInstance GetHandle() const noexcept;

	/**
	 * @brief Returns true if validation layers and the debug messenger are active.
	 * @return True if the Instance was successfully initialized with enableValidation=true.
	 */
	[[nodiscard]] bool IsValidationEnabled() const noexcept;

	/**
	 * @brief Returns true if this Instance currently owns a VkInstance handle.
	 * @return True if Initialize succeeded and the Instance has not been moved-from.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

private:
	VkInstance _instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;
	bool _validationEnabled = false;

	void Destroy() noexcept;
};

} // namespace virasa

#endif // VIRASA_RENDERER_CORE_INSTANCE_H
