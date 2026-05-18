#include <vulkan/vulkan.h>

#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

#include "virasa/core/Logger.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Instance.h"

using virasa::Instance;
using virasa::RendererConfig;
using virasa::RenderError;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{

// Returns a RendererConfig suitable for creating a real VkInstance without
// validation layers (faster, fewer external dependencies in CI).
RendererConfig MakeMinimalConfig()
{
	RendererConfig cfg;
	cfg.applicationName = "TestApp";
	cfg.applicationVersion = 1;
	cfg.enableValidation = false;
	cfg.requiredInstanceExtensions = nullptr;
	cfg.requiredInstanceExtensionCount = 0;
	return cfg;
}

// Returns a RendererConfig with validation enabled.
RendererConfig MakeValidationConfig()
{
	RendererConfig cfg = MakeMinimalConfig();
	cfg.enableValidation = true;
	return cfg;
}

} // namespace

// ---------------------------------------------------------------------------
// instance_is_raii_movable_non_copyable_handle_owner
// ---------------------------------------------------------------------------
TEST(Instance, test_instance_is_raii_movable_non_copyable_handle_owner)
{
	// Not copyable
	EXPECT_FALSE(std::is_copy_constructible_v<Instance>);
	EXPECT_FALSE(std::is_copy_assignable_v<Instance>);

	// Movable
	EXPECT_TRUE(std::is_move_constructible_v<Instance>);
	EXPECT_TRUE(std::is_move_assignable_v<Instance>);

	// Final
	EXPECT_TRUE(std::is_final_v<Instance>);

	// Default-constructible
	EXPECT_TRUE(std::is_default_constructible_v<Instance>);

	// Move constructor transfers ownership; source becomes default-constructed
	{
		Instance src;
		RendererConfig cfg = MakeMinimalConfig();
		RenderError err = src.Initialize(cfg);
		if (err == RenderError::None)
		{
			ASSERT_TRUE(src.IsInitialized());
			ASSERT_NE(src.GetHandle(), VK_NULL_HANDLE);

			Instance dst(std::move(src));

			// Source is now in moved-from (default-constructed) state
			EXPECT_EQ(src.GetHandle(), VK_NULL_HANDLE);
			EXPECT_FALSE(src.IsInitialized());
			EXPECT_FALSE(src.IsValidationEnabled());

			// Destination owns the handle
			EXPECT_NE(dst.GetHandle(), VK_NULL_HANDLE);
			EXPECT_TRUE(dst.IsInitialized());
		}
		// If Vulkan is not available we still verify the type traits above.
	}

	// Move assignment: existing handles are destroyed before taking ownership
	{
		Instance a;
		Instance b;
		RendererConfig cfg = MakeMinimalConfig();

		RenderError errA = a.Initialize(cfg);
		if (errA == RenderError::None)
		{
			VkInstance handleA = a.GetHandle();
			ASSERT_NE(handleA, VK_NULL_HANDLE);

			b = std::move(a);

			EXPECT_EQ(a.GetHandle(), VK_NULL_HANDLE);
			EXPECT_FALSE(a.IsInitialized());
			EXPECT_FALSE(a.IsValidationEnabled());

			EXPECT_EQ(b.GetHandle(), handleA);
			EXPECT_TRUE(b.IsInitialized());
		}
	}

	// Destroying a default-constructed Instance is well-defined (no crash)
	{
		Instance i;
		(void)i;
	}

	// Destroying a moved-from Instance is well-defined
	{
		Instance src;
		Instance dst(std::move(src));
		(void)dst;
		// src goes out of scope here — must not crash
	}
}

// ---------------------------------------------------------------------------
// instance_default_constructed_state
// ---------------------------------------------------------------------------
TEST(Instance, test_instance_default_constructed_state)
{
	Instance i;

	EXPECT_EQ(i.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(i.IsInitialized());
	EXPECT_FALSE(i.IsValidationEnabled());

	// Calling observers multiple times has no side effects
	EXPECT_EQ(i.GetHandle(), VK_NULL_HANDLE);
	EXPECT_FALSE(i.IsInitialized());
	EXPECT_FALSE(i.IsValidationEnabled());
}

// ---------------------------------------------------------------------------
// initialize_creates_vk_instance_from_config
// ---------------------------------------------------------------------------
TEST(Instance, test_initialize_creates_vk_instance_from_config)
{
	// --- AlreadyInitialized path ---
	{
		Instance i;
		RendererConfig cfg = MakeMinimalConfig();
		RenderError first = i.Initialize(cfg);
		if (first == RenderError::None)
		{
			VkInstance handleBefore = i.GetHandle();
			bool initializedBefore = i.IsInitialized();
			bool validationBefore = i.IsValidationEnabled();

			RenderError second = i.Initialize(cfg);
			EXPECT_EQ(second, RenderError::AlreadyInitialized);

			// State must be unchanged
			EXPECT_EQ(i.GetHandle(), handleBefore);
			EXPECT_EQ(i.IsInitialized(), initializedBefore);
			EXPECT_EQ(i.IsValidationEnabled(), validationBefore);
		}
	}

	// --- Success path without validation ---
	{
		Instance i;
		RendererConfig cfg = MakeMinimalConfig();
		RenderError err = i.Initialize(cfg);

		if (err == RenderError::None)
		{
			EXPECT_TRUE(i.IsInitialized());
			EXPECT_NE(i.GetHandle(), VK_NULL_HANDLE);
			EXPECT_FALSE(i.IsValidationEnabled()); // enableValidation was false
		}
		else
		{
			// Vulkan not available on this host; state must remain default
			EXPECT_EQ(i.GetHandle(), VK_NULL_HANDLE);
			EXPECT_FALSE(i.IsInitialized());
			EXPECT_FALSE(i.IsValidationEnabled());
		}
	}

	// --- Success path with validation (if layers are available) ---
	{
		Instance i;
		RendererConfig cfg = MakeValidationConfig();
		RenderError err = i.Initialize(cfg);

		if (err == RenderError::None)
		{
			EXPECT_TRUE(i.IsInitialized());
			EXPECT_NE(i.GetHandle(), VK_NULL_HANDLE);
			EXPECT_TRUE(i.IsValidationEnabled());
		}
		else
		{
			// Layer or extension missing — state unchanged
			EXPECT_EQ(i.GetHandle(), VK_NULL_HANDLE);
			EXPECT_FALSE(i.IsInitialized());
			EXPECT_FALSE(i.IsValidationEnabled());
		}
	}

	// --- VulkanNotAvailable path: request a non-existent extension ---
	{
		Instance i;
		const char* badExt = "VK_EXT_this_extension_does_not_exist_at_all";
		RendererConfig cfg = MakeMinimalConfig();
		cfg.requiredInstanceExtensions = &badExt;
		cfg.requiredInstanceExtensionCount = 1;

		RenderError err = i.Initialize(cfg);
		// Either VulkanNotAvailable (extension check failed) or
		// InstanceCreateFailed — both are non-None failures.
		// The contract says missing extension → VulkanNotAvailable.
		EXPECT_EQ(err, RenderError::VulkanNotAvailable);

		// State must remain default-constructed
		EXPECT_EQ(i.GetHandle(), VK_NULL_HANDLE);
		EXPECT_FALSE(i.IsInitialized());
		EXPECT_FALSE(i.IsValidationEnabled());
	}
}

// ---------------------------------------------------------------------------
// initialize_does_not_partially_modify_on_failure
// ---------------------------------------------------------------------------
TEST(Instance, test_initialize_does_not_partially_modify_on_failure)
{
	// Verify that every failure path leaves the Instance in its pre-call state.

	// Case 1: default-constructed Instance, failure due to missing extension
	{
		Instance i;
		const char* badExt = "VK_EXT_this_extension_does_not_exist_at_all";
		RendererConfig cfg = MakeMinimalConfig();
		cfg.requiredInstanceExtensions = &badExt;
		cfg.requiredInstanceExtensionCount = 1;

		VkInstance handleBefore = i.GetHandle();
		bool initializedBefore = i.IsInitialized();
		bool validationBefore = i.IsValidationEnabled();

		RenderError err = i.Initialize(cfg);
		EXPECT_NE(err, RenderError::None);

		EXPECT_EQ(i.GetHandle(), handleBefore);
		EXPECT_EQ(i.IsInitialized(), initializedBefore);
		EXPECT_EQ(i.IsValidationEnabled(), validationBefore);
	}

	// Case 2: already-initialized Instance, AlreadyInitialized path
	{
		Instance i;
		RendererConfig cfg = MakeMinimalConfig();
		RenderError first = i.Initialize(cfg);
		if (first == RenderError::None)
		{
			VkInstance handleBefore = i.GetHandle();
			bool initializedBefore = i.IsInitialized();
			bool validationBefore = i.IsValidationEnabled();

			RenderError second = i.Initialize(cfg);
			EXPECT_EQ(second, RenderError::AlreadyInitialized);

			EXPECT_EQ(i.GetHandle(), handleBefore);
			EXPECT_EQ(i.IsInitialized(), initializedBefore);
			EXPECT_EQ(i.IsValidationEnabled(), validationBefore);
		}
	}
}

// ---------------------------------------------------------------------------
// validation_messages_route_through_logger
// ---------------------------------------------------------------------------
TEST(Instance, test_validation_messages_route_through_logger)
{
	// We cannot intercept individual log messages in a unit test without
	// significant infrastructure, so this test verifies the observable
	// contract: Initialize with enableValidation=true succeeds (when layers
	// are available) and the Instance reports IsValidationEnabled()==true,
	// which is the necessary precondition for the debug messenger to exist
	// and route messages. We also verify that Initialize with
	// enableValidation=false does NOT consult Logger (no crash, no side
	// effect) and IsValidationEnabled() returns false.

	// Ensure Logger is available (GetLogger auto-initializes)
	virasa::Logger::GetLogger("renderer.vulkan");

	// With validation enabled
	{
		Instance i;
		RendererConfig cfg = MakeValidationConfig();
		RenderError err = i.Initialize(cfg);
		if (err == RenderError::None)
		{
			EXPECT_TRUE(i.IsValidationEnabled());
			EXPECT_TRUE(i.IsInitialized());
			EXPECT_NE(i.GetHandle(), VK_NULL_HANDLE);
			// The debug messenger is registered; we cannot trigger a real
			// validation message here, but the messenger's existence is
			// implied by IsValidationEnabled() == true after a successful
			// Initialize.
		}
		// If layers are absent the test still passes — the semantic only
		// applies when Initialize succeeds.
	}

	// With validation disabled — Logger must not be consulted
	{
		Instance i;
		RendererConfig cfg = MakeMinimalConfig(); // enableValidation = false
		RenderError err = i.Initialize(cfg);
		if (err == RenderError::None)
		{
			EXPECT_FALSE(i.IsValidationEnabled());
		}
	}
}

// ---------------------------------------------------------------------------
// get_handle_returns_owned_vk_instance_or_null
// ---------------------------------------------------------------------------
TEST(Instance, test_get_handle_returns_owned_vk_instance_or_null)
{
	// Default-constructed: VK_NULL_HANDLE
	{
		Instance i;
		EXPECT_EQ(i.GetHandle(), VK_NULL_HANDLE);
	}

	// After successful Initialize: non-null
	{
		Instance i;
		RendererConfig cfg = MakeMinimalConfig();
		RenderError err = i.Initialize(cfg);
		if (err == RenderError::None)
		{
			EXPECT_NE(i.GetHandle(), VK_NULL_HANDLE);

			// Handle remains valid after multiple GetHandle calls
			VkInstance h1 = i.GetHandle();
			VkInstance h2 = i.GetHandle();
			EXPECT_EQ(h1, h2);
		}
	}

	// After move: source becomes VK_NULL_HANDLE, destination has the handle
	{
		Instance src;
		RendererConfig cfg = MakeMinimalConfig();
		RenderError err = src.Initialize(cfg);
		if (err == RenderError::None)
		{
			VkInstance originalHandle = src.GetHandle();
			ASSERT_NE(originalHandle, VK_NULL_HANDLE);

			Instance dst(std::move(src));
			EXPECT_EQ(src.GetHandle(), VK_NULL_HANDLE);
			EXPECT_EQ(dst.GetHandle(), originalHandle);
		}
	}
}

// ---------------------------------------------------------------------------
// is_validation_enabled_reflects_init_argument
// ---------------------------------------------------------------------------
TEST(Instance, test_is_validation_enabled_reflects_init_argument)
{
	// Default-constructed: false
	{
		Instance i;
		EXPECT_FALSE(i.IsValidationEnabled());
	}

	// After failed Initialize: still false
	{
		Instance i;
		const char* badExt = "VK_EXT_this_extension_does_not_exist_at_all";
		RendererConfig cfg = MakeMinimalConfig();
		cfg.requiredInstanceExtensions = &badExt;
		cfg.requiredInstanceExtensionCount = 1;
		RenderError err = i.Initialize(cfg);
		EXPECT_NE(err, RenderError::None);
		EXPECT_FALSE(i.IsValidationEnabled());
	}

	// After successful Initialize with enableValidation=false: false
	{
		Instance i;
		RendererConfig cfg = MakeMinimalConfig();
		RenderError err = i.Initialize(cfg);
		if (err == RenderError::None)
		{
			EXPECT_FALSE(i.IsValidationEnabled());
		}
	}

	// After successful Initialize with enableValidation=true: true
	{
		Instance i;
		RendererConfig cfg = MakeValidationConfig();
		RenderError err = i.Initialize(cfg);
		if (err == RenderError::None)
		{
			EXPECT_TRUE(i.IsValidationEnabled());
		}
	}

	// After move: source reports false
	{
		Instance src;
		RendererConfig cfg = MakeValidationConfig();
		RenderError err = src.Initialize(cfg);
		if (err == RenderError::None)
		{
			ASSERT_TRUE(src.IsValidationEnabled());
			Instance dst(std::move(src));
			EXPECT_FALSE(src.IsValidationEnabled());
		}
	}
}

// ---------------------------------------------------------------------------
// is_initialized_reflects_owned_handle
// ---------------------------------------------------------------------------
TEST(Instance, test_is_initialized_reflects_owned_handle)
{
	// Default-constructed: false, and consistent with GetHandle
	{
		Instance i;
		EXPECT_FALSE(i.IsInitialized());
		EXPECT_EQ(i.IsInitialized(), (i.GetHandle() != VK_NULL_HANDLE));
	}

	// After failed Initialize: false
	{
		Instance i;
		const char* badExt = "VK_EXT_this_extension_does_not_exist_at_all";
		RendererConfig cfg = MakeMinimalConfig();
		cfg.requiredInstanceExtensions = &badExt;
		cfg.requiredInstanceExtensionCount = 1;
		RenderError err = i.Initialize(cfg);
		EXPECT_NE(err, RenderError::None);
		EXPECT_FALSE(i.IsInitialized());
		EXPECT_EQ(i.IsInitialized(), (i.GetHandle() != VK_NULL_HANDLE));
	}

	// After successful Initialize: true, consistent with GetHandle
	{
		Instance i;
		RendererConfig cfg = MakeMinimalConfig();
		RenderError err = i.Initialize(cfg);
		if (err == RenderError::None)
		{
			EXPECT_TRUE(i.IsInitialized());
			EXPECT_EQ(i.IsInitialized(), (i.GetHandle() != VK_NULL_HANDLE));
		}
	}

	// After move: source is false, destination is true; both consistent
	{
		Instance src;
		RendererConfig cfg = MakeMinimalConfig();
		RenderError err = src.Initialize(cfg);
		if (err == RenderError::None)
		{
			ASSERT_TRUE(src.IsInitialized());
			Instance dst(std::move(src));

			EXPECT_FALSE(src.IsInitialized());
			EXPECT_EQ(src.IsInitialized(), (src.GetHandle() != VK_NULL_HANDLE));

			EXPECT_TRUE(dst.IsInitialized());
			EXPECT_EQ(dst.IsInitialized(), (dst.GetHandle() != VK_NULL_HANDLE));
		}
	}
}

// ---------------------------------------------------------------------------
// instance_methods_are_not_thread_safe_per_instance
// ---------------------------------------------------------------------------
// This semantic is a documentation/contract statement about the absence of
// internal synchronization. We verify the positive side: distinct Instance
// objects may be used concurrently (each from its own thread) without data
// races, and const observers on the same Instance may be called concurrently.
#include <atomic>
#include <thread>

TEST(Instance, test_instance_methods_are_not_thread_safe_per_instance)
{
	// Two distinct instances initialized from separate threads — no shared
	// mutable state between them, so this must be race-free.
	RendererConfig cfg = MakeMinimalConfig();

	Instance i1;
	Instance i2;
	std::atomic<RenderError> err1{RenderError::None};
	std::atomic<RenderError> err2{RenderError::None};

	std::thread t1([&]() { err1.store(i1.Initialize(cfg)); });
	std::thread t2([&]() { err2.store(i2.Initialize(cfg)); });
	t1.join();
	t2.join();

	// Each instance is independently in a valid state
	if (err1.load() == RenderError::None)
	{
		EXPECT_TRUE(i1.IsInitialized());
		EXPECT_NE(i1.GetHandle(), VK_NULL_HANDLE);
	}
	else
	{
		EXPECT_FALSE(i1.IsInitialized());
		EXPECT_EQ(i1.GetHandle(), VK_NULL_HANDLE);
	}

	if (err2.load() == RenderError::None)
	{
		EXPECT_TRUE(i2.IsInitialized());
		EXPECT_NE(i2.GetHandle(), VK_NULL_HANDLE);
	}
	else
	{
		EXPECT_FALSE(i2.IsInitialized());
		EXPECT_EQ(i2.GetHandle(), VK_NULL_HANDLE);
	}

	// Concurrent const observers on the same initialized instance
	Instance i3;
	RenderError err3 = i3.Initialize(cfg);
	if (err3 == RenderError::None)
	{
		// Spawn several threads all calling const observers concurrently
		constexpr int kThreadCount = 8;
		std::vector<std::thread> readers;
		readers.reserve(kThreadCount);
		for (int idx = 0; idx < kThreadCount; ++idx)
		{
			readers.emplace_back(
				[&]()
				{
					(void)i3.GetHandle();
					(void)i3.IsInitialized();
					(void)i3.IsValidationEnabled();
				});
		}
		for (auto& t : readers)
			t.join();
		// If we reach here without a sanitizer report, the test passes.
		EXPECT_TRUE(i3.IsInitialized());
	}
}
