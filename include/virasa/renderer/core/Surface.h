#ifndef VIRASA_RENDERER_CORE_SURFACE_H
#define VIRASA_RENDERER_CORE_SURFACE_H

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/window/Platform.h"

#include "vulkan/vulkan.h"

namespace virasa
{

/**
 * @brief Owns and manages a VkSurfaceKHR created from a Platform window.
 *
 * Surface is a RAII wrapper around a VkSurfaceKHR handle. It is
 * default-constructible (owning no handle), movable, and non-copyable.
 * Initialization is performed via Initialize(), which delegates surface
 * creation to the Platform. Physical-device-dependent state (format,
 * present mode, capabilities) is populated by QueryFormatsAndModes() and
 * may be refreshed with RefreshCapabilities().
 */
class Surface final
{
public:
	/// @brief Default-constructs a Surface that owns no Vulkan handle.
	Surface() noexcept = default;

	/// @brief Destroys the owned VkSurfaceKHR, if any.
	~Surface() noexcept;

	// Non-copyable
	Surface(const Surface&) = delete;
	Surface& operator=(const Surface&) = delete;

	// Movable
	Surface(Surface&& other) noexcept;
	Surface& operator=(Surface&& other) noexcept;

	/**
	 * @brief Creates a VkSurfaceKHR by delegating to platform.CreateSurface.
	 * @param instance An initialized Instance whose VkInstance handle is used.
	 * @param platform An initialized Platform used to create the surface.
	 * @return RenderError::None on success, RenderError::SurfaceCreateFailed on failure.
	 */
	[[nodiscard]] RenderError Initialize(const Instance& instance, virasa::window::Platform& platform);

	/**
	 * @brief Queries surface formats, present modes, and capabilities from the physical device.
	 * @param physical_device A valid VkPhysicalDevice handle.
	 * @param config Renderer configuration (e.g. preferMailbox).
	 */
	void QueryFormatsAndModes(VkPhysicalDevice physical_device, const RendererConfig& config);

	/**
	 * @brief Re-queries only the surface capabilities from the physical device.
	 * @param physical_device A valid VkPhysicalDevice handle.
	 */
	void RefreshCapabilities(VkPhysicalDevice physical_device);

	/**
	 * @brief Returns the owned VkSurfaceKHR handle, or VK_NULL_HANDLE if none.
	 * @return The owned surface handle.
	 */
	[[nodiscard]] VkSurfaceKHR GetHandle() const noexcept;

	/**
	 * @brief Returns the cached surface format selected by QueryFormatsAndModes.
	 * @return The selected VkSurfaceFormatKHR.
	 */
	[[nodiscard]] VkSurfaceFormatKHR GetFormat() const noexcept;

	/**
	 * @brief Returns the cached present mode selected by QueryFormatsAndModes.
	 * @return The selected VkPresentModeKHR.
	 */
	[[nodiscard]] VkPresentModeKHR GetPresentMode() const noexcept;

	/**
	 * @brief Returns a reference to the cached surface capabilities.
	 * @return The most recently cached VkSurfaceCapabilitiesKHR.
	 */
	[[nodiscard]] const VkSurfaceCapabilitiesKHR& GetCapabilities() const noexcept;

	/**
	 * @brief Returns true if this Surface owns a VkSurfaceKHR handle.
	 * @return true if initialized, false otherwise.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

	/**
	 * @brief Returns true if QueryFormatsAndModes has been called since the last successful Initialize.
	 * @return true if formats have been queried, false otherwise.
	 */
	[[nodiscard]] bool AreFormatsQueried() const noexcept;

private:
	VkInstance _instance{ VK_NULL_HANDLE };
	VkSurfaceKHR _surface{ VK_NULL_HANDLE };
	VkSurfaceFormatKHR _format{};
	VkPresentModeKHR _presentMode{ VK_PRESENT_MODE_FIFO_KHR };
	VkSurfaceCapabilitiesKHR _capabilities{};
	bool _formatsQueried{ false };
};

} // namespace virasa

#endif // VIRASA_RENDERER_CORE_SURFACE_H
