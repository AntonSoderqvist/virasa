#ifndef VIRASA_RENDERER_TYPES_H
#define VIRASA_RENDERER_TYPES_H

#include <cstdint>

namespace virasa
{

/**
 * @brief Error codes returned by renderer operations.
 *
 * RenderError::None is the unique success value. All other values represent
 * distinct failure modes. The underlying type is uint8_t.
 */
enum class RenderError : uint8_t
{
	None = 0,
	AlreadyInitialized,
	NotInitialized,
	VulkanNotAvailable,
	InstanceCreateFailed,
	SurfaceCreateFailed
};

/**
 * @brief Renderer-wide configuration shared by every contract in the virasa.renderer subtree.
 *
 * RendererConfig is a plain aggregate struct. All pointer members are non-owning;
 * the caller must keep the referenced data alive for the duration of any call
 * that receives this struct.
 *
 * Consumers that create a VkInstance must set VkApplicationInfo::apiVersion to
 * VK_API_VERSION_1_3.
 */
struct RendererConfig
{
	/// @brief Null-terminated application name passed to Vulkan via VkApplicationInfo.
	const char* applicationName = "Virasa";

	/// @brief Application version passed to Vulkan via VkApplicationInfo.
	uint32_t applicationVersion = 0;

	/// @brief When true, the Khronos validation layer and VK_EXT_debug_utils are requested.
	bool enableValidation = true;

	/**
	 * @brief Pointer to a contiguous array of null-terminated Vulkan instance extension names
	 *        required by the caller. May be nullptr when requiredInstanceExtensionCount is 0.
	 */
	const char* const* requiredInstanceExtensions = nullptr;

	/// @brief Number of strings in the requiredInstanceExtensions array.
	uint32_t requiredInstanceExtensionCount = 0;

	/**
	 * @brief When true, prefer VK_PRESENT_MODE_MAILBOX_KHR over VK_PRESENT_MODE_FIFO_KHR
	 *        if both are supported. Advisory only — falls back to FIFO when mailbox is unavailable.
	 */
	bool preferMailbox = false;
};

} // namespace virasa

#endif // VIRASA_RENDERER_TYPES_H
