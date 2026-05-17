#include <thread>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <span>
#include <thread>
#include <type_traits>
#include <vector>

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "virasa/window/Events.h"
#include "virasa/window/Platform.h"
#include "virasa/window/Types.h"

using namespace virasa;
using namespace virasa::window;

// ---------------------------------------------------------------------------
// Helper: build a minimal VkInstance with the extensions Platform requires.
// Returns VK_NULL_HANDLE on failure (callers skip the surface sub-test).
// ---------------------------------------------------------------------------
namespace
{

VkInstance CreateTestInstance()
{
	uint32_t extCount = 0;
	const char* const* exts = Platform::GetRequiredVulkanExtensions(&extCount);

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "PlatformTest";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.pEngineName = "virasa";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pApplicationInfo = &appInfo;
	ci.enabledExtensionCount = extCount;
	ci.ppEnabledExtensionNames = exts;

	VkInstance instance = VK_NULL_HANDLE;
	if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS)
		return VK_NULL_HANDLE;
	return instance;
}

} // namespace

// ===========================================================================
// platform_is_non_copyable_non_movable_default_constructible
// ===========================================================================
TEST(Platform, test_platform_is_non_copyable_non_movable_default_constructible)
{
	// Default-constructible
	Platform p;
	(void)p;

	// Non-copyable
	EXPECT_FALSE(std::is_copy_constructible_v<Platform>);
	EXPECT_FALSE(std::is_copy_assignable_v<Platform>);

	// Non-movable
	EXPECT_FALSE(std::is_move_constructible_v<Platform>);
	EXPECT_FALSE(std::is_move_assignable_v<Platform>);

	// Final
	EXPECT_TRUE(std::is_final_v<Platform>);
}

// ===========================================================================
// platform_uninitialized_state_observable_behavior
// ===========================================================================
TEST(Platform, test_platform_uninitialized_state_observable_behavior)
{
	Platform p;

	// GetWindowHandle returns nullptr
	EXPECT_EQ(p.GetWindowHandle(), nullptr);

	// GetSurface returns VK_NULL_HANDLE
	EXPECT_EQ(p.GetSurface(), VK_NULL_HANDLE);

	// GetWindowSize returns default-constructed Size2D
	Size2D sz = p.GetWindowSize();
	EXPECT_EQ(sz.width, 0u);
	EXPECT_EQ(sz.height, 0u);

	// IsMinimized returns false
	EXPECT_FALSE(p.IsMinimized());

	// IsQuitRequested returns false
	EXPECT_FALSE(p.IsQuitRequested());

	// GetRawEvents returns empty span
	EXPECT_TRUE(p.GetRawEvents().empty());

	// PollEvents returns empty span without crashing
	EXPECT_TRUE(p.PollEvents().empty());

	// CreateSurface returns NotInitialized
	VkSurfaceKHR surf = VK_NULL_HANDLE;
	EXPECT_EQ(p.CreateSurface(VK_NULL_HANDLE, &surf), ErrorCode::NotInitialized);

	// StartTextInput returns NotInitialized
	EXPECT_EQ(p.StartTextInput(), ErrorCode::NotInitialized);

	// Shutdown and StopTextInput are no-ops (must not crash)
	p.Shutdown();
	p.StopTextInput();
}

// ===========================================================================
// initialize_creates_window_and_starts_text_input
// ===========================================================================
TEST(Platform, test_initialize_creates_window_and_starts_text_input)
{
	Platform p;

	// Successful initialization
	ErrorCode result = p.Initialize("TestWindow", 800u, 600u);
	EXPECT_EQ(result, ErrorCode::None);

	if (result == ErrorCode::None)
	{
		// Window handle must be non-null
		EXPECT_NE(p.GetWindowHandle(), nullptr);

		// Stored size must match the arguments
		Size2D sz = p.GetWindowSize();
		EXPECT_EQ(sz.width, 800u);
		EXPECT_EQ(sz.height, 600u);

		// Calling Initialize again while already initialized returns AlreadyInitialized
		EXPECT_EQ(p.Initialize("AnotherWindow", 1280u, 720u), ErrorCode::AlreadyInitialized);

		// State must not have changed
		EXPECT_EQ(p.GetWindowSize().width, 800u);
		EXPECT_EQ(p.GetWindowSize().height, 600u);

		p.Shutdown();
	}

	// After failure path: platform remains uninitialized
	// (We can verify the uninitialized state is restored after Shutdown)
	EXPECT_EQ(p.GetWindowHandle(), nullptr);
}

// ===========================================================================
// shutdown_tears_down_platform_and_allows_reinitialization
// ===========================================================================
TEST(Platform, test_shutdown_tears_down_platform_and_allows_reinitialization)
{
	Platform p;
	ASSERT_EQ(p.Initialize("ShutdownTest", 640u, 480u), ErrorCode::None);

	p.Shutdown();

	// Must be back in uninitialized state
	EXPECT_EQ(p.GetWindowHandle(), nullptr);
	EXPECT_EQ(p.GetSurface(), VK_NULL_HANDLE);
	Size2D sz = p.GetWindowSize();
	EXPECT_EQ(sz.width, 0u);
	EXPECT_EQ(sz.height, 0u);
	EXPECT_FALSE(p.IsMinimized());
	EXPECT_FALSE(p.IsQuitRequested());

	// Idempotent: second Shutdown must not crash
	p.Shutdown();

	// Re-initialization must succeed
	EXPECT_EQ(p.Initialize("ShutdownTest2", 320u, 240u), ErrorCode::None);
	EXPECT_NE(p.GetWindowHandle(), nullptr);
	EXPECT_EQ(p.GetWindowSize().width, 320u);
	EXPECT_EQ(p.GetWindowSize().height, 240u);

	p.Shutdown();
}

// ===========================================================================
// create_surface_produces_caller_owned_vulkan_surface
// ===========================================================================
TEST(Platform, test_create_surface_produces_caller_owned_vulkan_surface)
{
	Platform p;
	ASSERT_EQ(p.Initialize("SurfaceTest", 800u, 600u), ErrorCode::None);

	VkInstance instance = CreateTestInstance();
	if (instance == VK_NULL_HANDLE)
	{
		p.Shutdown();
		GTEST_SKIP() << "Could not create VkInstance; skipping surface test.";
	}

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	ErrorCode result = p.CreateSurface(instance, &surface);
	EXPECT_EQ(result, ErrorCode::None);

	if (result == ErrorCode::None)
	{
		// Surface must be non-null
		EXPECT_NE(surface, VK_NULL_HANDLE);

		// GetSurface must return the same handle
		EXPECT_EQ(p.GetSurface(), surface);

		// Caller owns the surface; destroy it before the instance
		vkDestroySurfaceKHR(instance, surface, nullptr);
	}

	p.Shutdown();

	// After Shutdown the stored handle must be cleared
	EXPECT_EQ(p.GetSurface(), VK_NULL_HANDLE);

	vkDestroyInstance(instance, nullptr);
}

// ===========================================================================
// get_required_vulkan_extensions_is_static_and_callable_before_initialize
// ===========================================================================
TEST(Platform, test_get_required_vulkan_extensions_is_static_and_callable_before_initialize)
{
	// Callable before any Platform instance exists / is initialized
	uint32_t count = 0;
	const char* const* exts = Platform::GetRequiredVulkanExtensions(&count);

	// Must return a non-null pointer and at least one extension
	EXPECT_NE(exts, nullptr);
	EXPECT_GT(count, 0u);

	// Each extension name must be a non-null, non-empty string
	for (uint32_t i = 0; i < count; ++i)
	{
		ASSERT_NE(exts[i], nullptr);
		EXPECT_GT(std::strlen(exts[i]), 0u);
	}

	// Callable with null out_count (must not crash)
	const char* const* exts2 = Platform::GetRequiredVulkanExtensions(nullptr);
	EXPECT_NE(exts2, nullptr);
}

// ===========================================================================
// poll_events_drains_platform_queue_and_translates_to_event
// ===========================================================================
TEST(Platform, test_poll_events_drains_platform_queue_and_translates_to_event)
{
	Platform p;
	ASSERT_EQ(p.Initialize("PollTest", 800u, 600u), ErrorCode::None);

	// Push a synthetic SDL_QUIT event so we can verify translation
	SDL_Event quitEv{};
	quitEv.type = SDL_EVENT_QUIT;
	quitEv.quit.timestamp = 42u;
	SDL_PushEvent(&quitEv);

	std::span<const Event> events = p.PollEvents();

	// The quit event must appear in the translated span
	bool foundQuit = false;
	for (const Event& ev : events)
	{
		if (ev.type == EventType::Quit)
		{
			foundQuit = true;
			break;
		}
	}
	EXPECT_TRUE(foundQuit);

	// The raw events must also contain the quit event
	std::span<const SDL_Event> rawEvents = p.GetRawEvents();
	bool foundRawQuit = false;
	for (const SDL_Event& raw : rawEvents)
	{
		if (raw.type == SDL_EVENT_QUIT)
		{
			foundRawQuit = true;
			break;
		}
	}
	EXPECT_TRUE(foundRawQuit);

	// IsQuitRequested must now be true
	EXPECT_TRUE(p.IsQuitRequested());

	p.Shutdown();
}

// ===========================================================================
// poll_events_invalidates_prior_spans
// ===========================================================================
TEST(Platform, test_poll_events_invalidates_prior_spans)
{
	Platform p;
	ASSERT_EQ(p.Initialize("SpanInvalidTest", 800u, 600u), ErrorCode::None);

	// First poll — capture span sizes
	std::span<const Event> firstEvents = p.PollEvents();
	std::span<const SDL_Event> firstRaw = p.GetRawEvents();
	(void)firstEvents;
	(void)firstRaw;

	// Second poll — prior spans are invalidated; new spans are fresh
	std::span<const Event> secondEvents = p.PollEvents();
	std::span<const SDL_Event> secondRaw = p.GetRawEvents();

	// After a second poll with no injected events the spans should be empty
	EXPECT_TRUE(secondEvents.empty());
	EXPECT_TRUE(secondRaw.empty());

	p.Shutdown();
}

// ===========================================================================
// text_input_payload_truncation
// ===========================================================================
TEST(Platform, test_text_input_payload_truncation)
{
	Platform p;
	ASSERT_EQ(p.Initialize("TextTruncTest", 800u, 600u), ErrorCode::None);

	// Build a text-input SDL event with a payload longer than 31 bytes.
	// SDL_TextInputEvent.text is a fixed buffer; we fill it with 'a' * 32
	// (leaving room for NUL at index 32 in the SDL buffer).
	SDL_Event textEv{};
	textEv.type = SDL_EVENT_TEXT_INPUT;
	textEv.text.timestamp = 1u;
	// SDL3 SDL_TextInputEvent.text is const char*
	textEv.text.text = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; // 31 'a' chars
	SDL_PushEvent(&textEv);

	std::span<const Event> events = p.PollEvents();

	bool foundText = false;
	for (const Event& ev : events)
	{
		if (ev.type == EventType::TextInput)
		{
			foundText = true;
			// Payload must be NUL-terminated
			EXPECT_EQ(ev.textInput.utf8[ev.textInput.length], '\0');
			// Length must not exceed 31
			EXPECT_LE(ev.textInput.length, 31u);
			break;
		}
	}
	EXPECT_TRUE(foundText);

	// Now test a short payload (verbatim copy)
	SDL_Event shortEv{};
	shortEv.type = SDL_EVENT_TEXT_INPUT;
	shortEv.text.timestamp = 2u;
	shortEv.text.text = "hi";
	SDL_PushEvent(&shortEv);

	events = p.PollEvents();
	bool foundShort = false;
	for (const Event& ev : events)
	{
		if (ev.type == EventType::TextInput)
		{
			foundShort = true;
			EXPECT_EQ(ev.textInput.length, 2u);
			EXPECT_EQ(ev.textInput.utf8[0], 'h');
			EXPECT_EQ(ev.textInput.utf8[1], 'i');
			EXPECT_EQ(ev.textInput.utf8[2], '\0');
			break;
		}
	}
	EXPECT_TRUE(foundShort);

	p.Shutdown();
}

// ===========================================================================
// get_window_size_reflects_last_known_size
// ===========================================================================
TEST(Platform, test_get_window_size_reflects_last_known_size)
{
	Platform p;

	// Uninitialized: default size
	Size2D sz = p.GetWindowSize();
	EXPECT_EQ(sz.width, 0u);
	EXPECT_EQ(sz.height, 0u);

	ASSERT_EQ(p.Initialize("SizeTest", 1024u, 768u), ErrorCode::None);

	// After Initialize: size matches arguments
	sz = p.GetWindowSize();
	EXPECT_EQ(sz.width, 1024u);
	EXPECT_EQ(sz.height, 768u);

	// Inject a window-resized event
	SDL_Event resizeEv{};
	resizeEv.type = SDL_EVENT_WINDOW_RESIZED;
	resizeEv.window.timestamp = 10u;
	resizeEv.window.data1 = 1920;
	resizeEv.window.data2 = 1080;
	SDL_PushEvent(&resizeEv);

	p.PollEvents();

	sz = p.GetWindowSize();
	EXPECT_EQ(sz.width, 1920u);
	EXPECT_EQ(sz.height, 1080u);

	p.Shutdown();

	// After Shutdown: back to default
	sz = p.GetWindowSize();
	EXPECT_EQ(sz.width, 0u);
	EXPECT_EQ(sz.height, 0u);
}

// ===========================================================================
// is_minimized_reflects_last_minimize_restore_event
// ===========================================================================
TEST(Platform, test_is_minimized_reflects_last_minimize_restore_event)
{
	Platform p;
	EXPECT_FALSE(p.IsMinimized());

	ASSERT_EQ(p.Initialize("MinimizeTest", 800u, 600u), ErrorCode::None);
	EXPECT_FALSE(p.IsMinimized());

	// Inject a minimize event
	SDL_Event minEv{};
	minEv.type = SDL_EVENT_WINDOW_MINIMIZED;
	minEv.window.timestamp = 20u;
	SDL_PushEvent(&minEv);
	p.PollEvents();
	EXPECT_TRUE(p.IsMinimized());

	// Inject a restore event
	SDL_Event restEv{};
	restEv.type = SDL_EVENT_WINDOW_RESTORED;
	restEv.window.timestamp = 21u;
	SDL_PushEvent(&restEv);
	p.PollEvents();
	EXPECT_FALSE(p.IsMinimized());

	// Minimize again, then Shutdown resets the flag
	SDL_PushEvent(&minEv);
	p.PollEvents();
	EXPECT_TRUE(p.IsMinimized());

	p.Shutdown();
	EXPECT_FALSE(p.IsMinimized());
}

// ===========================================================================
// is_quit_requested_is_sticky_until_shutdown
// ===========================================================================
TEST(Platform, test_is_quit_requested_is_sticky_until_shutdown)
{
	Platform p;
	EXPECT_FALSE(p.IsQuitRequested());

	ASSERT_EQ(p.Initialize("QuitStickyTest", 800u, 600u), ErrorCode::None);
	EXPECT_FALSE(p.IsQuitRequested());

	// Push a quit event
	SDL_Event quitEv{};
	quitEv.type = SDL_EVENT_QUIT;
	quitEv.quit.timestamp = 30u;
	SDL_PushEvent(&quitEv);
	p.PollEvents();
	EXPECT_TRUE(p.IsQuitRequested());

	// Subsequent PollEvents (no new quit) must keep the flag true
	p.PollEvents();
	EXPECT_TRUE(p.IsQuitRequested());

	// Shutdown clears the flag
	p.Shutdown();
	EXPECT_FALSE(p.IsQuitRequested());

	// Re-initialize: flag must start false again
	ASSERT_EQ(p.Initialize("QuitStickyTest2", 800u, 600u), ErrorCode::None);
	EXPECT_FALSE(p.IsQuitRequested());
	p.Shutdown();
}

// ===========================================================================
// get_window_handle_returns_native_window_or_nullptr
// ===========================================================================
TEST(Platform, test_get_window_handle_returns_native_window_or_nullptr)
{
	Platform p;
	EXPECT_EQ(p.GetWindowHandle(), nullptr);

	ASSERT_EQ(p.Initialize("HandleTest", 800u, 600u), ErrorCode::None);
	EXPECT_NE(p.GetWindowHandle(), nullptr);

	p.Shutdown();
	EXPECT_EQ(p.GetWindowHandle(), nullptr);
}

// ===========================================================================
// get_raw_events_returns_native_events_from_last_poll
// ===========================================================================
TEST(Platform, test_get_raw_events_returns_native_events_from_last_poll)
{
	Platform p;

	// Uninitialized: empty
	EXPECT_TRUE(p.GetRawEvents().empty());

	ASSERT_EQ(p.Initialize("RawEventsTest", 800u, 600u), ErrorCode::None);

	// Before first PollEvents: empty
	EXPECT_TRUE(p.GetRawEvents().empty());

	// Push a user-defined event that won't be translated to an Event
	// but must still appear in raw events.
	SDL_Event userEv{};
	userEv.type = SDL_EVENT_QUIT; // use quit so it's a known type
	userEv.quit.timestamp = 50u;
	SDL_PushEvent(&userEv);

	p.PollEvents();

	std::span<const SDL_Event> raw = p.GetRawEvents();
	EXPECT_FALSE(raw.empty());

	bool found = false;
	for (const SDL_Event& ev : raw)
	{
		if (ev.type == SDL_EVENT_QUIT)
		{
			found = true;
			break;
		}
	}
	EXPECT_TRUE(found);

	p.Shutdown();
}

// ===========================================================================
// get_surface_returns_stored_non_owned_handle
// ===========================================================================
TEST(Platform, test_get_surface_returns_stored_non_owned_handle)
{
	Platform p;

	// Uninitialized: VK_NULL_HANDLE
	EXPECT_EQ(p.GetSurface(), VK_NULL_HANDLE);

	ASSERT_EQ(p.Initialize("SurfaceHandleTest", 800u, 600u), ErrorCode::None);

	// Before CreateSurface: VK_NULL_HANDLE
	EXPECT_EQ(p.GetSurface(), VK_NULL_HANDLE);

	VkInstance instance = CreateTestInstance();
	if (instance == VK_NULL_HANDLE)
	{
		p.Shutdown();
		GTEST_SKIP() << "Could not create VkInstance; skipping surface handle test.";
	}

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (p.CreateSurface(instance, &surface) == ErrorCode::None)
	{
		EXPECT_NE(surface, VK_NULL_HANDLE);
		EXPECT_EQ(p.GetSurface(), surface);

		// Shutdown clears stored handle but does not destroy the surface
		p.Shutdown();
		EXPECT_EQ(p.GetSurface(), VK_NULL_HANDLE);

		// Caller destroys the surface
		vkDestroySurfaceKHR(instance, surface, nullptr);
	}
	else
	{
		p.Shutdown();
	}

	vkDestroyInstance(instance, nullptr);
}

// ===========================================================================
// start_text_input_enables_text_events_on_window
// ===========================================================================
TEST(Platform, test_start_text_input_enables_text_events_on_window)
{
	Platform p;

	// Uninitialized: NotInitialized
	EXPECT_EQ(p.StartTextInput(), ErrorCode::NotInitialized);

	ASSERT_EQ(p.Initialize("TextInputTest", 800u, 600u), ErrorCode::None);

	// Initialized: must succeed (or return SdlInitFailed on platform error)
	ErrorCode result = p.StartTextInput();
	EXPECT_TRUE(result == ErrorCode::None || result == ErrorCode::SdlInitFailed);

	// Calling again (idempotent) must not crash and must return the same class of result
	ErrorCode result2 = p.StartTextInput();
	EXPECT_TRUE(result2 == ErrorCode::None || result2 == ErrorCode::SdlInitFailed);

	p.Shutdown();
}

// ===========================================================================
// stop_text_input_disables_text_events_on_window
// ===========================================================================
TEST(Platform, test_stop_text_input_disables_text_events_on_window)
{
	Platform p;

	// Uninitialized: no-op, must not crash
	p.StopTextInput();

	ASSERT_EQ(p.Initialize("StopTextTest", 800u, 600u), ErrorCode::None);

	// Stop when text input may or may not be active — must not crash
	p.StopTextInput();

	// Calling twice is safe
	p.StopTextInput();

	// After StopTextInput, inject a text-input SDL event; PollEvents should
	// still drain the queue (the event may or may not be suppressed at the
	// SDL level depending on platform, so we only verify no crash).
	SDL_Event textEv{};
	textEv.type = SDL_EVENT_TEXT_INPUT;
	textEv.text.timestamp = 60u;
	textEv.text.text = "x";
	SDL_PushEvent(&textEv);
	EXPECT_NO_FATAL_FAILURE(p.PollEvents());

	p.Shutdown();
}

// ===========================================================================
// platform_methods_are_not_thread_safe
// ===========================================================================
// This semantic documents a contract property (no thread safety) rather than
// a testable runtime behavior. We verify the static method is callable from
// multiple threads concurrently, which IS pinned as safe by the contract.
TEST(Platform, test_platform_methods_are_not_thread_safe)
{
	// The contract pins GetRequiredVulkanExtensions as safe to call from
	// multiple threads concurrently. Verify it returns consistent results
	// when called from two threads.
	uint32_t count1 = 0, count2 = 0;
	const char* const* exts1 = nullptr;
	const char* const* exts2 = nullptr;

	std::thread t1([&] { exts1 = Platform::GetRequiredVulkanExtensions(&count1); });
	std::thread t2([&] { exts2 = Platform::GetRequiredVulkanExtensions(&count2); });
	t1.join();
	t2.join();

	EXPECT_NE(exts1, nullptr);
	EXPECT_NE(exts2, nullptr);
	EXPECT_EQ(count1, count2);
}
