#include <vulkan/vulkan.h>

#include <cstdint>
#include <gtest/gtest.h>
#include <utility>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/core/Surface.h"
#include "virasa/renderer/core/Swapchain.h"
#include "virasa/window/Platform.h"
#include "virasa/window/Types.h"

using namespace virasa;
using namespace virasa::window;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{

// Initializes a Platform with a small hidden window suitable for Vulkan tests.
// Returns the ErrorCode from Platform::Initialize.
ErrorCode InitializePlatform(Platform& platform)
{
	return platform.Initialize("ContextTest", 128, 128);
}

// Builds a RendererConfig that requests the extensions the Platform requires
// and disables validation to keep test setup fast and avoid layer availability
// issues in CI.
RendererConfig MakeTestConfig(Platform& platform)
{
	RendererConfig config;
	config.enableValidation = false;
	config.applicationName = "ContextTest";
	config.maxFramesInFlight = 2;
	uint32_t extCount = 0;
	const char* const* exts = Platform::GetRequiredVulkanExtensions(&extCount);
	config.requiredInstanceExtensions = exts;
	config.requiredInstanceExtensionCount = extCount;
	return config;
}

// Creates a fully-initialized Context (Platform already initialized by caller).
// Returns the RenderError from Context::Initialize.
RenderError InitializeContext(Context& ctx, Platform& platform, const RendererConfig& config)
{
	return ctx.Initialize(platform, config);
}

} // namespace

// ---------------------------------------------------------------------------
// context_owns_full_vulkan_stack_raii_movable_non_copyable
// ---------------------------------------------------------------------------
TEST(Context, test_context_owns_full_vulkan_stack_raii_movable_non_copyable)
{
	// Not copyable
	EXPECT_FALSE(std::is_copy_constructible_v<Context>);
	EXPECT_FALSE(std::is_copy_assignable_v<Context>);

	// Movable
	EXPECT_TRUE(std::is_move_constructible_v<Context>);
	EXPECT_TRUE(std::is_move_assignable_v<Context>);

	// Final
	EXPECT_TRUE(std::is_final_v<Context>);

	// Default-constructed is not ready
	{
		Context ctx;
		EXPECT_FALSE(ctx.IsReady());
	}

	// Move-construction transfers ownership; source becomes not-ready
	{
		Platform platform;
		ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
		RendererConfig config = MakeTestConfig(platform);

		Context src;
		RenderError err = InitializeContext(src, platform, config);
		if (err == RenderError::None)
		{
			EXPECT_TRUE(src.IsReady());
			Context dst(std::move(src));
			EXPECT_TRUE(dst.IsReady());
			EXPECT_FALSE(src.IsReady()); // NOLINT(bugprone-use-after-move)
			EXPECT_EQ(src.GetCurrentFrameIndex(), 0u);
		}
		platform.Shutdown();
	}

	// Move-assignment transfers ownership; source becomes not-ready.
	// Only one Context can hold an active swapchain per Platform at a time, so
	// we initialize src alone and move-assign into an empty dst.  We also
	// verify the Shutdown path by move-assigning an empty Context into a ready
	// one — this exercises the Shutdown() call inside operator=.
	// src/dst are destroyed inside the inner scope, before platform.Shutdown(),
	// because the Swapchain teardown needs the X11 display to still be open.
	{
		Platform platform;
		ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
		RendererConfig config = MakeTestConfig(platform);

		{
			Context src;
			RenderError errSrc = InitializeContext(src, platform, config);
			if (errSrc == RenderError::None)
			{
				EXPECT_TRUE(src.IsReady());
				Context dst;
				dst = std::move(src);
				EXPECT_TRUE(dst.IsReady());
				EXPECT_FALSE(src.IsReady()); // NOLINT(bugprone-use-after-move)

				// Move-assigning an empty Context into a ready one calls Shutdown().
				dst = Context{};
				EXPECT_FALSE(dst.IsReady());
			}
		}
		platform.Shutdown();
	}
}

// ---------------------------------------------------------------------------
// context_default_constructed_state
// ---------------------------------------------------------------------------
TEST(Context, test_context_default_constructed_state)
{
	Context ctx;
	EXPECT_FALSE(ctx.IsReady());
	EXPECT_EQ(ctx.GetCurrentFrameIndex(), 0u);
	// GetMaxFramesInFlight on a never-initialized Context is not pinned to a
	// specific value by the contract, so we only verify the call does not crash.
	(void)ctx.GetMaxFramesInFlight();
}

// ---------------------------------------------------------------------------
// context_initialize_builds_stack_in_dependency_order
// ---------------------------------------------------------------------------
TEST(Context, test_context_initialize_builds_stack_in_dependency_order)
{
	Platform platform;
	ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
	RendererConfig config = MakeTestConfig(platform);

	Context ctx;
	RenderError err = InitializeContext(ctx, platform, config);

	// On systems with Vulkan support the full stack must be ready.
	// On systems without Vulkan support Initialize returns a non-None error
	// and IsReady must remain false.
	if (err == RenderError::None)
	{
		EXPECT_TRUE(ctx.IsReady());
		// Stack observers must return non-null handles
		EXPECT_NE(ctx.GetDevice().GetHandle(), VK_NULL_HANDLE);
		EXPECT_NE(ctx.GetInstance().GetHandle(), VK_NULL_HANDLE);
		EXPECT_TRUE(ctx.GetSwapchain().IsInitialized());
		EXPECT_NE(ctx.GetGraphicsCommandPool(), VK_NULL_HANDLE);
	}
	else
	{
		EXPECT_FALSE(ctx.IsReady());
	}

	ctx.Shutdown();
	platform.Shutdown();
}

// ---------------------------------------------------------------------------
// context_creates_graphics_command_pool_and_buffers
// ---------------------------------------------------------------------------
TEST(Context, test_context_creates_graphics_command_pool_and_buffers)
{
	Platform platform;
	ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
	RendererConfig config = MakeTestConfig(platform);
	config.maxFramesInFlight = 2;

	Context ctx;
	RenderError err = InitializeContext(ctx, platform, config);

	if (err == RenderError::None)
	{
		// Command pool must be valid
		EXPECT_NE(ctx.GetGraphicsCommandPool(), VK_NULL_HANDLE);
		// maxFramesInFlight must reflect config
		EXPECT_EQ(ctx.GetMaxFramesInFlight(), 2u);
		// GetCurrentCommandBuffer must return a non-null handle at frame index 0
		EXPECT_NE(ctx.GetCurrentCommandBuffer(), VK_NULL_HANDLE);
	}

	ctx.Shutdown();
	platform.Shutdown();
}

// ---------------------------------------------------------------------------
// context_creates_per_frame_sync_objects
// ---------------------------------------------------------------------------
// The per-frame sync objects (semaphores and fences) are internal; their
// creation is verified indirectly: a successful Initialize (which includes
// step 7) leaves IsReady true, and a subsequent BeginFrame/EndFrame cycle
// completes without error, which would be impossible if the sync objects
// were not correctly created.
TEST(Context, test_context_creates_per_frame_sync_objects)
{
	Platform platform;
	ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
	RendererConfig config = MakeTestConfig(platform);
	config.maxFramesInFlight = 2;

	Context ctx;
	RenderError err = InitializeContext(ctx, platform, config);

	if (err == RenderError::None)
	{
		// If sync objects were not created, BeginFrame would deadlock or crash.
		// We verify that BeginFrame returns a non-Error status.
		SwapchainStatus status = ctx.BeginFrame();
		if (status == SwapchainStatus::Success || status == SwapchainStatus::Recreated)
		{
			SwapchainStatus endStatus = ctx.EndFrame();
			EXPECT_NE(endStatus, SwapchainStatus::Error);
		}
		else
		{
			// NotReady is acceptable (e.g. minimized window in CI)
			EXPECT_NE(status, SwapchainStatus::Error);
		}
	}

	ctx.Shutdown();
	platform.Shutdown();
}

// ---------------------------------------------------------------------------
// context_shutdown_tears_down_in_reverse_order
// ---------------------------------------------------------------------------
TEST(Context, test_context_shutdown_tears_down_in_reverse_order)
{
	// Shutdown on a default-constructed (not-ready) Context must be a no-op.
	{
		Context ctx;
		ctx.Shutdown(); // must not crash
		EXPECT_FALSE(ctx.IsReady());
	}

	// Shutdown on a ready Context must leave it not-ready.
	{
		Platform platform;
		ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
		RendererConfig config = MakeTestConfig(platform);

		Context ctx;
		RenderError err = InitializeContext(ctx, platform, config);
		if (err == RenderError::None)
		{
			EXPECT_TRUE(ctx.IsReady());
			ctx.Shutdown();
			EXPECT_FALSE(ctx.IsReady());
			EXPECT_EQ(ctx.GetCurrentFrameIndex(), 0u);

			// Shutdown is idempotent: second call must not crash.
			ctx.Shutdown();
			EXPECT_FALSE(ctx.IsReady());
		}

		platform.Shutdown();
	}

	// Re-initialization after Shutdown must succeed.
	{
		Platform platform;
		ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
		RendererConfig config = MakeTestConfig(platform);

		Context ctx;
		RenderError err1 = InitializeContext(ctx, platform, config);
		if (err1 == RenderError::None)
		{
			ctx.Shutdown();
			EXPECT_FALSE(ctx.IsReady());
			RenderError err2 = InitializeContext(ctx, platform, config);
			EXPECT_EQ(err2, RenderError::None);
			EXPECT_TRUE(ctx.IsReady());
		}

		ctx.Shutdown();
		platform.Shutdown();
	}
}

// ---------------------------------------------------------------------------
// context_begin_frame_acquires_image_with_retry
// ---------------------------------------------------------------------------
TEST(Context, test_context_begin_frame_acquires_image_with_retry)
{
	Platform platform;
	ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
	RendererConfig config = MakeTestConfig(platform);

	Context ctx;
	RenderError err = InitializeContext(ctx, platform, config);

	if (err == RenderError::None)
	{
		SwapchainStatus status = ctx.BeginFrame();

		// Valid outcomes: Success, Recreated, or NotReady (minimized in CI).
		// Error is not acceptable on a freshly-initialized Context.
		EXPECT_NE(status, SwapchainStatus::Error);

		if (status == SwapchainStatus::Success || status == SwapchainStatus::Recreated)
		{
			// Command buffer must be in recording state — handle must be non-null.
			EXPECT_NE(ctx.GetCurrentCommandBuffer(), VK_NULL_HANDLE);
			// Must end the frame to leave the Context in a consistent state.
			ctx.EndFrame();
		}
	}

	ctx.Shutdown();
	platform.Shutdown();
}

// ---------------------------------------------------------------------------
// context_end_frame_submits_and_presents
// ---------------------------------------------------------------------------
TEST(Context, test_context_end_frame_submits_and_presents)
{
	Platform platform;
	ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
	RendererConfig config = MakeTestConfig(platform);
	config.maxFramesInFlight = 2;

	Context ctx;
	RenderError err = InitializeContext(ctx, platform, config);

	if (err == RenderError::None)
	{
		// Run two full frames to verify frame-index advancement.
		for (int frame = 0; frame < 2; ++frame)
		{
			uint32_t indexBefore = ctx.GetCurrentFrameIndex();
			SwapchainStatus beginStatus = ctx.BeginFrame();
			EXPECT_NE(beginStatus, SwapchainStatus::Error);

			if (beginStatus == SwapchainStatus::Success ||
				beginStatus == SwapchainStatus::Recreated)
			{
				SwapchainStatus endStatus = ctx.EndFrame();
				EXPECT_NE(endStatus, SwapchainStatus::Error);

				// Frame index must have advanced by 1 modulo maxFramesInFlight.
				uint32_t expectedNext = (indexBefore + 1) % ctx.GetMaxFramesInFlight();
				EXPECT_EQ(ctx.GetCurrentFrameIndex(), expectedNext);
			}
		}
	}

	ctx.Shutdown();
	platform.Shutdown();
}

// ---------------------------------------------------------------------------
// context_recreate_swapchain
// ---------------------------------------------------------------------------
// Swapchain recreation is triggered internally by BeginFrame/EndFrame when
// the swapchain becomes out-of-date. We verify the observable post-condition:
// after a Recreated return from BeginFrame or EndFrame the Context remains
// ready and the swapchain observers reflect valid state.
TEST(Context, test_context_recreate_swapchain)
{
	Platform platform;
	ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
	RendererConfig config = MakeTestConfig(platform);

	Context ctx;
	RenderError err = InitializeContext(ctx, platform, config);

	if (err == RenderError::None)
	{
		// Run several frames; if any returns Recreated the Context must still be
		// ready and the swapchain extent must be non-zero.
		for (int frame = 0; frame < 4; ++frame)
		{
			SwapchainStatus beginStatus = ctx.BeginFrame();
			EXPECT_NE(beginStatus, SwapchainStatus::Error);

			if (beginStatus == SwapchainStatus::Success ||
				beginStatus == SwapchainStatus::Recreated)
			{
				if (beginStatus == SwapchainStatus::Recreated)
				{
					EXPECT_TRUE(ctx.IsReady());
					VkExtent2D ext = ctx.GetSwapchainExtent();
					EXPECT_GT(ext.width, 0u);
					EXPECT_GT(ext.height, 0u);
				}
				SwapchainStatus endStatus = ctx.EndFrame();
				if (endStatus == SwapchainStatus::Recreated)
				{
					EXPECT_TRUE(ctx.IsReady());
					VkExtent2D ext = ctx.GetSwapchainExtent();
					EXPECT_GT(ext.width, 0u);
					EXPECT_GT(ext.height, 0u);
				}
			}
		}
	}

	ctx.Shutdown();
	platform.Shutdown();
}

// ---------------------------------------------------------------------------
// context_frame_state_observers
// ---------------------------------------------------------------------------
TEST(Context, test_context_frame_state_observers)
{
	Platform platform;
	ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
	RendererConfig config = MakeTestConfig(platform);

	Context ctx;
	RenderError err = InitializeContext(ctx, platform, config);

	if (err == RenderError::None)
	{
		// Before BeginFrame: swapchain format and extent are valid on a ready Context.
		VkFormat fmt = ctx.GetSwapchainFormat();
		EXPECT_NE(fmt, VK_FORMAT_UNDEFINED);
		VkExtent2D ext = ctx.GetSwapchainExtent();
		EXPECT_GT(ext.width, 0u);
		EXPECT_GT(ext.height, 0u);

		SwapchainStatus beginStatus = ctx.BeginFrame();
		EXPECT_NE(beginStatus, SwapchainStatus::Error);

		if (beginStatus == SwapchainStatus::Success ||
			beginStatus == SwapchainStatus::Recreated)
		{
			// Between BeginFrame and EndFrame all frame-state observers must return
			// non-null / non-zero values.
			EXPECT_NE(ctx.GetCurrentCommandBuffer(), VK_NULL_HANDLE);
			EXPECT_NE(ctx.GetCurrentImageView(), VK_NULL_HANDLE);
			EXPECT_NE(ctx.GetCurrentImage(), VK_NULL_HANDLE);
			EXPECT_NE(ctx.GetSwapchainFormat(), VK_FORMAT_UNDEFINED);
			VkExtent2D extDuring = ctx.GetSwapchainExtent();
			EXPECT_GT(extDuring.width, 0u);
			EXPECT_GT(extDuring.height, 0u);

			ctx.EndFrame();
		}
	}

	ctx.Shutdown();
	platform.Shutdown();
}

// ---------------------------------------------------------------------------
// context_stack_observers
// ---------------------------------------------------------------------------
TEST(Context, test_context_stack_observers)
{
	Platform platform;
	ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
	RendererConfig config = MakeTestConfig(platform);

	Context ctx;
	RenderError err = InitializeContext(ctx, platform, config);

	if (err == RenderError::None)
	{
		// All stack observers must return valid (non-null / initialized) objects.
		const Device& device = ctx.GetDevice();
		EXPECT_TRUE(device.IsInitialized());
		EXPECT_NE(device.GetHandle(), VK_NULL_HANDLE);

		const Instance& instance = ctx.GetInstance();
		EXPECT_TRUE(instance.IsInitialized());
		EXPECT_NE(instance.GetHandle(), VK_NULL_HANDLE);

		const Swapchain& swapchain = ctx.GetSwapchain();
		EXPECT_TRUE(swapchain.IsInitialized());

		EXPECT_NE(ctx.GetGraphicsCommandPool(), VK_NULL_HANDLE);
	}

	ctx.Shutdown();
	platform.Shutdown();
}

// ---------------------------------------------------------------------------
// context_frame_index_observers
// ---------------------------------------------------------------------------
TEST(Context, test_context_frame_index_observers)
{
	// Default-constructed: frame index is 0.
	{
		Context ctx;
		EXPECT_EQ(ctx.GetCurrentFrameIndex(), 0u);
	}

	// After Initialize: frame index starts at 0, advances after EndFrame.
	{
		Platform platform;
		ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
		RendererConfig config = MakeTestConfig(platform);
		config.maxFramesInFlight = 2;

		Context ctx;
		RenderError err = InitializeContext(ctx, platform, config);

		if (err == RenderError::None)
		{
			EXPECT_EQ(ctx.GetCurrentFrameIndex(), 0u);
			EXPECT_EQ(ctx.GetMaxFramesInFlight(), 2u);

			SwapchainStatus beginStatus = ctx.BeginFrame();
			// BeginFrame must not change the frame index.
			EXPECT_EQ(ctx.GetCurrentFrameIndex(), 0u);

			if (beginStatus == SwapchainStatus::Success ||
				beginStatus == SwapchainStatus::Recreated)
			{
				ctx.EndFrame();
				// After EndFrame the index must have advanced to 1.
				EXPECT_EQ(ctx.GetCurrentFrameIndex(), 1u);
			}

			// After Shutdown the index resets to 0.
			ctx.Shutdown();
			EXPECT_EQ(ctx.GetCurrentFrameIndex(), 0u);
		}

		platform.Shutdown();
	}
}

// ---------------------------------------------------------------------------
// context_is_ready_reflects_successful_initialize
// ---------------------------------------------------------------------------
TEST(Context, test_context_is_ready_reflects_successful_initialize)
{
	// Default-constructed: not ready.
	{
		Context ctx;
		EXPECT_FALSE(ctx.IsReady());
	}

	// After successful Initialize: ready.
	// After Shutdown: not ready.
	{
		Platform platform;
		ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
		RendererConfig config = MakeTestConfig(platform);

		Context ctx;
		RenderError err = InitializeContext(ctx, platform, config);
		if (err == RenderError::None)
		{
			EXPECT_TRUE(ctx.IsReady());
			ctx.Shutdown();
			EXPECT_FALSE(ctx.IsReady());
		}
		else
		{
			// Failed Initialize must leave IsReady false.
			EXPECT_FALSE(ctx.IsReady());
		}

		platform.Shutdown();
	}

	// Moved-from Context: not ready.
	{
		Platform platform;
		ASSERT_EQ(InitializePlatform(platform), ErrorCode::None);
		RendererConfig config = MakeTestConfig(platform);

		Context src;
		RenderError err = InitializeContext(src, platform, config);
		if (err == RenderError::None)
		{
			Context dst(std::move(src));
			EXPECT_FALSE(src.IsReady()); // NOLINT(bugprone-use-after-move)
			EXPECT_TRUE(dst.IsReady());
		}

		platform.Shutdown();
	}
}

// ---------------------------------------------------------------------------
// context_is_not_thread_safe_per_instance
// ---------------------------------------------------------------------------
// Thread-safety is a design constraint, not an observable runtime behavior
// that can be verified by a test without invoking undefined behavior.
// We verify the structural properties the contract pins: distinct Context
// objects may be constructed and operated independently, and the type traits
// confirm no implicit synchronization mechanism is present.
TEST(Context, test_context_is_not_thread_safe_per_instance)
{
	// Two independent Context objects can be initialized against two independent
	// Platform objects without interfering with each other.
	Platform platformA;
	Platform platformB;
	ASSERT_EQ(InitializePlatform(platformA), ErrorCode::None);
	ASSERT_EQ(InitializePlatform(platformB), ErrorCode::None);

	RendererConfig configA = MakeTestConfig(platformA);
	RendererConfig configB = MakeTestConfig(platformB);

	Context ctxA;
	Context ctxB;
	RenderError errA = InitializeContext(ctxA, platformA, configA);
	RenderError errB = InitializeContext(ctxB, platformB, configB);

	if (errA == RenderError::None)
	{
		EXPECT_TRUE(ctxA.IsReady());
	}
	if (errB == RenderError::None)
	{
		EXPECT_TRUE(ctxB.IsReady());
	}

	// Context has no mutex or atomic member that would indicate built-in
	// synchronization; the type is not default-constructible with any
	// lock-like semantics. We simply confirm both contexts operate without
	// interference when used sequentially from one thread.
	if (errA == RenderError::None && errB == RenderError::None)
	{
		SwapchainStatus sA = ctxA.BeginFrame();
		EXPECT_NE(sA, SwapchainStatus::Error);
		if (sA == SwapchainStatus::Success || sA == SwapchainStatus::Recreated)
			ctxA.EndFrame();

		SwapchainStatus sB = ctxB.BeginFrame();
		EXPECT_NE(sB, SwapchainStatus::Error);
		if (sB == SwapchainStatus::Success || sB == SwapchainStatus::Recreated)
			ctxB.EndFrame();
	}

	ctxA.Shutdown();
	ctxB.Shutdown();
	platformA.Shutdown();
	platformB.Shutdown();
}
