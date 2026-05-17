#ifndef VIRASA_WINDOW_PLATFORM_H
#define VIRASA_WINDOW_PLATFORM_H

#include <vulkan/vulkan.h>

#include <span>
#include <vector>

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "virasa/window/Events.h"
#include "virasa/window/Types.h"

namespace virasa::window
{

/**
 * @brief Manages the SDL3 window and Vulkan surface lifecycle for the application.
 *
 * Platform is the first system initialized and the last destroyed. It is
 * default-constructible, non-copyable, and non-movable. A default-constructed
 * Platform is in the uninitialized state until Initialize() is called.
 */
class Platform final
{
	public:
	/// @brief Default constructor. Produces a Platform in the uninitialized state.
	Platform() = default;

	/// @brief Destructor. Performs the same observable work as Shutdown().
	~Platform();

	Platform(const Platform&) = delete;
	Platform& operator=(const Platform&) = delete;
	Platform(Platform&&) = delete;
	Platform& operator=(Platform&&) = delete;

	/**
	 * @brief Transition from uninitialized to initialized state.
	 *
	 * Starts the SDL video subsystem, creates a Vulkan-capable resizable window,
	 * stores the pixel dimensions, and enables text input.
	 *
	 * @return ErrorCode::None on success.
	 * @return ErrorCode::AlreadyInitialized if already initialized.
	 * @return ErrorCode::SdlInitFailed if SDL video subsystem startup failed.
	 * @return ErrorCode::WindowCreateFailed if window creation failed.
	 */
	[[nodiscard]] ErrorCode Initialize();

	/**
	 * @brief Transition from initialized to uninitialized state.
	 *
	 * Stops text input, destroys the window, releases the SDL video subsystem,
	 * clears stored state, and invalidates any outstanding event spans.
	 * Idempotent: safe to call when already uninitialized.
	 */
	void Shutdown();

	/**
	 * @brief Create a Vulkan surface for the platform window.
	 *
	 * The caller owns the returned surface and must destroy it via
	 * vkDestroySurfaceKHR before destroying the VkInstance.
	 *
	 * @param instance A valid VkInstance with the required extensions enabled.
	 * @param out_surface Non-null pointer; receives the created VkSurfaceKHR on success.
	 * @return ErrorCode::None on success.
	 * @return ErrorCode::NotInitialized if Platform is uninitialized.
	 * @return ErrorCode::SurfaceCreateFailed if the underlying call failed.
	 */
	[[nodiscard]] ErrorCode CreateSurface(VkInstance instance, VkSurfaceKHR* out_surface);

	/**
	 * @brief Return the Vulkan instance extensions required for surface creation.
	 *
	 * Callable at any time, including before Initialize() and after destruction.
	 * The returned pointer and strings are valid for the lifetime of the SDL library.
	 *
	 * @param out_count When non-null, receives the number of extension name strings.
	 * @return Pointer to an array of null-terminated extension name strings.
	 */
	[[nodiscard]] static const char* const* GetRequiredVulkanExtensions(uint32_t* out_count);

	/**
	 * @brief Drain all pending platform events and return translated Event values.
	 *
	 * Clears prior event and raw-event spans, polls all pending SDL events,
	 * translates recognized events to Event values, and updates internal state
	 * (window size, minimized flag, quit-requested flag).
	 *
	 * @return Span of translated Event values valid until the next PollEvents call.
	 */
	[[nodiscard]] std::span<const Event> PollEvents();

	/**
	 * @brief Return the most recently recorded window size.
	 *
	 * Does not query the platform layer; returns cached state.
	 *
	 * @return Cached Size2D. Returns {0, 0} in the uninitialized state.
	 */
	[[nodiscard]] Size2D GetWindowSize() const;

	/**
	 * @brief Return whether the window is currently minimized.
	 *
	 * @return true if the last processed window event was a minimize event.
	 */
	[[nodiscard]] bool IsMinimized() const;

	/**
	 * @brief Return whether a quit event has been received during this session.
	 *
	 * The flag is sticky: once set it remains true until Shutdown().
	 *
	 * @return true if a quit-class event has been translated since Initialize().
	 */
	[[nodiscard]] bool IsQuitRequested() const;

	/**
	 * @brief Return the native SDL_Window handle.
	 *
	 * @return Non-owning pointer to the SDL_Window, or nullptr when uninitialized.
	 */
	[[nodiscard]] SDL_Window* GetWindowHandle() const;

	/**
	 * @brief Return the raw SDL_Event values collected by the last PollEvents call.
	 *
	 * @return Span of SDL_Event values valid until the next PollEvents call.
	 */
	[[nodiscard]] std::span<const SDL_Event> GetRawEvents() const;

	/**
	 * @brief Return the most recently stored Vulkan surface handle.
	 *
	 * @return The VkSurfaceKHR written by the last successful CreateSurface call,
	 *         or VK_NULL_HANDLE if none.
	 */
	[[nodiscard]] VkSurfaceKHR GetSurface() const;

	/**
	 * @brief Enable text input event generation on the platform window.
	 *
	 * @return ErrorCode::None on success.
	 * @return ErrorCode::NotInitialized if Platform is uninitialized.
	 * @return ErrorCode::SdlInitFailed if the underlying platform call failed.
	 */
	[[nodiscard]] ErrorCode StartTextInput();

	/**
	 * @brief Disable text input event generation on the platform window.
	 *
	 * No-op when uninitialized or when text input is not currently enabled.
	 */
	void StopTextInput();

	private:
	/// Translate an SDL_Keycode to our KeyCode enum.
	KeyCode TranslateSdlKeycode(SDL_Keycode sdl_key) const noexcept;

	/// Translate an SDL mouse button index to our MouseButton enum.
	MouseButton TranslateSdlMouseButton(uint8_t sdl_button) const noexcept;

	SDL_Window* _window = nullptr;
	VkSurfaceKHR _surface = VK_NULL_HANDLE;
	Size2D _windowSize{};
	bool _initialized = false;
	bool _minimized = false;
	bool _quitRequested = false;

	std::vector<Event> _events;
	std::vector<SDL_Event> _rawEvents;
};

} // namespace virasa::window

#endif // VIRASA_WINDOW_PLATFORM_H
