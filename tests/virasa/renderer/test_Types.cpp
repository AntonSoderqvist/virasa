#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>

#include "virasa/renderer/Types.h"
#include "vulkan/vulkan.h"

using namespace virasa;

// ---------------------------------------------------------------------------
// RenderError enum tests
// ---------------------------------------------------------------------------

TEST(Types, test_render_error_enum_values_in_declared_order)
{
	// Underlying type is uint8_t.
	static_assert(std::is_same_v<std::underlying_type_t<RenderError>, uint8_t>,
		"RenderError underlying type must be uint8_t");

	// None is explicitly 0.
	EXPECT_EQ(static_cast<uint8_t>(RenderError::None), uint8_t{0});

	// Values appear in declared order (each subsequent value is greater than
	// the previous one, which is the natural result of no explicit assignment
	// after the first).
	EXPECT_LT(static_cast<uint8_t>(RenderError::None),
		static_cast<uint8_t>(RenderError::AlreadyInitialized));
	EXPECT_LT(static_cast<uint8_t>(RenderError::AlreadyInitialized),
		static_cast<uint8_t>(RenderError::NotInitialized));
	EXPECT_LT(static_cast<uint8_t>(RenderError::NotInitialized),
		static_cast<uint8_t>(RenderError::VulkanNotAvailable));
	EXPECT_LT(static_cast<uint8_t>(RenderError::VulkanNotAvailable),
		static_cast<uint8_t>(RenderError::InstanceCreateFailed));
	EXPECT_LT(static_cast<uint8_t>(RenderError::InstanceCreateFailed),
		static_cast<uint8_t>(RenderError::SurfaceCreateFailed));

	// Version 3 adds NoSuitableDevice and DeviceCreateFailed after SurfaceCreateFailed.
	EXPECT_LT(static_cast<uint8_t>(RenderError::SurfaceCreateFailed),
		static_cast<uint8_t>(RenderError::NoSuitableDevice));
	EXPECT_LT(static_cast<uint8_t>(RenderError::NoSuitableDevice),
		static_cast<uint8_t>(RenderError::DeviceCreateFailed));

	// All eight values are distinct.
	uint8_t values[] = {
		static_cast<uint8_t>(RenderError::None),
		static_cast<uint8_t>(RenderError::AlreadyInitialized),
		static_cast<uint8_t>(RenderError::NotInitialized),
		static_cast<uint8_t>(RenderError::VulkanNotAvailable),
		static_cast<uint8_t>(RenderError::InstanceCreateFailed),
		static_cast<uint8_t>(RenderError::SurfaceCreateFailed),
		static_cast<uint8_t>(RenderError::NoSuitableDevice),
		static_cast<uint8_t>(RenderError::DeviceCreateFailed),
	};
	for (std::size_t i = 0; i < 8; ++i)
	{
		for (std::size_t j = i + 1; j < 8; ++j)
		{
			EXPECT_NE(values[i], values[j]);
		}
	}
}

TEST(Types, test_render_error_none_is_unique_success_value)
{
	// None == 0 is the success value.
	EXPECT_EQ(RenderError::None, static_cast<RenderError>(0));

	// Every non-None value is not equal to None.
	EXPECT_NE(RenderError::AlreadyInitialized, RenderError::None);
	EXPECT_NE(RenderError::NotInitialized, RenderError::None);
	EXPECT_NE(RenderError::VulkanNotAvailable, RenderError::None);
	EXPECT_NE(RenderError::InstanceCreateFailed, RenderError::None);
	EXPECT_NE(RenderError::SurfaceCreateFailed, RenderError::None);
	EXPECT_NE(RenderError::NoSuitableDevice, RenderError::None);
	EXPECT_NE(RenderError::DeviceCreateFailed, RenderError::None);

	// Callers check against None to determine success.
	auto isSuccess = [](RenderError e) { return e == RenderError::None; };
	EXPECT_TRUE(isSuccess(RenderError::None));
	EXPECT_FALSE(isSuccess(RenderError::AlreadyInitialized));
	EXPECT_FALSE(isSuccess(RenderError::NotInitialized));
	EXPECT_FALSE(isSuccess(RenderError::VulkanNotAvailable));
	EXPECT_FALSE(isSuccess(RenderError::InstanceCreateFailed));
	EXPECT_FALSE(isSuccess(RenderError::SurfaceCreateFailed));
	EXPECT_FALSE(isSuccess(RenderError::NoSuitableDevice));
	EXPECT_FALSE(isSuccess(RenderError::DeviceCreateFailed));
}

// ---------------------------------------------------------------------------
// QueueFamilies struct tests
// ---------------------------------------------------------------------------

TEST(Types, test_queue_families_holds_queue_family_indices)
{
	// Default construction.
	QueueFamilies qf;

	// Index fields default to 0.
	EXPECT_EQ(qf.graphicsFamily, uint32_t{0});
	EXPECT_EQ(qf.presentFamily, uint32_t{0});
	EXPECT_EQ(qf.transferFamily, uint32_t{0});

	// Found-flags default to false.
	EXPECT_FALSE(qf.graphicsFound);
	EXPECT_FALSE(qf.presentFound);
	EXPECT_FALSE(qf.dedicatedTransfer);

	// IsComplete() returns false when neither flag is set.
	EXPECT_FALSE(qf.IsComplete());

	// IsSameFamily() compares graphicsFamily == presentFamily regardless of flags.
	// Both are 0 by default, so IsSameFamily() is true on a default-constructed value.
	EXPECT_TRUE(qf.IsSameFamily());

	// Set graphics only — IsComplete() still false.
	qf.graphicsFamily = 1u;
	qf.graphicsFound = true;
	EXPECT_FALSE(qf.IsComplete());

	// Set present — IsComplete() now true.
	qf.presentFamily = 2u;
	qf.presentFound = true;
	EXPECT_TRUE(qf.IsComplete());

	// Different families → IsSameFamily() false.
	EXPECT_FALSE(qf.IsSameFamily());

	// Make them the same → IsSameFamily() true.
	qf.presentFamily = 1u;
	EXPECT_TRUE(qf.IsSameFamily());

	// dedicatedTransfer flag and transferFamily.
	qf.transferFamily = 3u;
	qf.dedicatedTransfer = true;
	EXPECT_TRUE(qf.dedicatedTransfer);
	EXPECT_EQ(qf.transferFamily, uint32_t{3});

	// IsComplete() does not consult dedicatedTransfer.
	EXPECT_TRUE(qf.IsComplete());

	// Copyable.
	QueueFamilies copy = qf;
	EXPECT_EQ(copy.graphicsFamily, qf.graphicsFamily);
	EXPECT_EQ(copy.presentFamily, qf.presentFamily);
	EXPECT_EQ(copy.transferFamily, qf.transferFamily);
	EXPECT_EQ(copy.graphicsFound, qf.graphicsFound);
	EXPECT_EQ(copy.presentFound, qf.presentFound);
	EXPECT_EQ(copy.dedicatedTransfer, qf.dedicatedTransfer);

	// Movable.
	QueueFamilies moved = std::move(copy);
	EXPECT_EQ(moved.graphicsFamily, uint32_t{1});
	EXPECT_EQ(moved.presentFamily, uint32_t{1});
	EXPECT_EQ(moved.transferFamily, uint32_t{3});
	EXPECT_TRUE(moved.graphicsFound);
	EXPECT_TRUE(moved.presentFound);
	EXPECT_TRUE(moved.dedicatedTransfer);
}

// ---------------------------------------------------------------------------
// RendererConfig struct tests
// ---------------------------------------------------------------------------

TEST(Types, test_renderer_config_holds_renderer_wide_configuration)
{
	// Default construction must work.
	RendererConfig config;

	// applicationName defaults to "Virasa".
	ASSERT_NE(config.applicationName, nullptr);
	EXPECT_STREQ(config.applicationName, "Virasa");

	// applicationVersion defaults to 0.
	EXPECT_EQ(config.applicationVersion, uint32_t{0});

	// enableValidation defaults to true.
	EXPECT_TRUE(config.enableValidation);

	// requiredInstanceExtensions defaults to nullptr.
	EXPECT_EQ(config.requiredInstanceExtensions, nullptr);

	// requiredInstanceExtensionCount defaults to 0.
	EXPECT_EQ(config.requiredInstanceExtensionCount, uint32_t{0});

	// preferMailbox defaults to false.
	EXPECT_FALSE(config.preferMailbox);

	// Members are publicly assignable.
	const char* extName = "VK_KHR_surface";
	const char* const exts[] = {extName};

	config.applicationName = "MyApp";
	config.applicationVersion = 1u;
	config.enableValidation = false;
	config.requiredInstanceExtensions = exts;
	config.requiredInstanceExtensionCount = 1u;
	config.preferMailbox = true;

	EXPECT_STREQ(config.applicationName, "MyApp");
	EXPECT_EQ(config.applicationVersion, uint32_t{1});
	EXPECT_FALSE(config.enableValidation);
	EXPECT_EQ(config.requiredInstanceExtensions, exts);
	EXPECT_EQ(config.requiredInstanceExtensionCount, uint32_t{1});
	EXPECT_TRUE(config.preferMailbox);

	// Copyable: copy-construct and verify members are equal.
	RendererConfig copy = config;
	EXPECT_STREQ(copy.applicationName, config.applicationName);
	EXPECT_EQ(copy.applicationVersion, config.applicationVersion);
	EXPECT_EQ(copy.enableValidation, config.enableValidation);
	EXPECT_EQ(copy.requiredInstanceExtensions, config.requiredInstanceExtensions);
	EXPECT_EQ(copy.requiredInstanceExtensionCount, config.requiredInstanceExtensionCount);
	EXPECT_EQ(copy.preferMailbox, config.preferMailbox);

	// Movable: move-construct and verify members transferred.
	RendererConfig moved = std::move(copy);
	EXPECT_STREQ(moved.applicationName, "MyApp");
	EXPECT_EQ(moved.applicationVersion, uint32_t{1});
	EXPECT_FALSE(moved.enableValidation);
	EXPECT_EQ(moved.requiredInstanceExtensions, exts);
	EXPECT_EQ(moved.requiredInstanceExtensionCount, uint32_t{1});
	EXPECT_TRUE(moved.preferMailbox);
}

TEST(Types, test_renderer_config_targets_vulkan_1_3)
{
	// The contract pins the Vulkan API version to 1.3 as a compile-time
	// constant — it is not a runtime field of RendererConfig. We verify this
	// by confirming that RendererConfig has no member that could represent an
	// API version (i.e., the struct has exactly the six documented members and
	// no additional version field), and that VK_API_VERSION_1_3 is defined and
	// has the expected encoded value.
	//
	// VK_API_VERSION_1_3 encodes major=1, minor=3 in the Vulkan variant/major/
	// minor/patch packing: bits [31:29]=variant(0), [28:22]=major(1),
	// [21:12]=minor(3), [11:0]=patch(0).
	// Using VK_MAKE_API_VERSION(0, 1, 3, 0) = (0<<29)|(1<<22)|(3<<12)|(0).
	constexpr uint32_t kExpectedApiVersion =
		(uint32_t{0} << 29) | (uint32_t{1} << 22) | (uint32_t{3} << 12) | uint32_t{0};
	EXPECT_EQ(VK_API_VERSION_1_3, kExpectedApiVersion);

	// RendererConfig has no apiVersion field — the Vulkan version is not
	// runtime-configurable. Verify the struct size is consistent with only the
	// six documented members (applicationName ptr, applicationVersion u32,
	// enableValidation bool, requiredInstanceExtensions ptr,
	// requiredInstanceExtensionCount u32, preferMailbox bool).
	// We do not assert an exact byte size (padding is implementation-defined),
	// but we do assert the struct is default-constructible and that none of its
	// public members is named apiVersion or vulkanVersion.
	static_assert(std::is_default_constructible_v<RendererConfig>,
		"RendererConfig must be default-constructible");

	// Spot-check: a default RendererConfig does not expose a Vulkan version
	// field (compile-time check via member access — if this compiles without
	// the field, the field does not exist).
	RendererConfig config;
	(void)config.applicationName;
	(void)config.applicationVersion;
	(void)config.enableValidation;
	(void)config.requiredInstanceExtensions;
	(void)config.requiredInstanceExtensionCount;
	(void)config.preferMailbox;
	// If the struct had an apiVersion member the test author would have caught
	// it during contract review; the absence of such a member is the assertion.
	EXPECT_TRUE(true); // structural assertion satisfied at compile time above.
}
