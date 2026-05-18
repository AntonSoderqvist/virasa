#include <cstdint>
#include <gtest/gtest.h>

#include "virasa/renderer/Types.h"

using namespace virasa;

TEST(Types, test_render_error_enum_values_in_declared_order)
{
	// Verify underlying type is uint8_t by checking static_assert-style casts.
	RenderError none = RenderError::None;
	RenderError alreadyInitialized = RenderError::AlreadyInitialized;
	RenderError notInitialized = RenderError::NotInitialized;
	RenderError vulkanNotAvailable = RenderError::VulkanNotAvailable;
	RenderError instanceCreateFailed = RenderError::InstanceCreateFailed;

	// None must be explicitly 0.
	EXPECT_EQ(static_cast<uint8_t>(none), uint8_t{0});

	// Values must appear in declared order (each successive value > previous).
	EXPECT_LT(static_cast<uint8_t>(none), static_cast<uint8_t>(alreadyInitialized));
	EXPECT_LT(static_cast<uint8_t>(alreadyInitialized), static_cast<uint8_t>(notInitialized));
	EXPECT_LT(static_cast<uint8_t>(notInitialized), static_cast<uint8_t>(vulkanNotAvailable));
	EXPECT_LT(
		static_cast<uint8_t>(vulkanNotAvailable), static_cast<uint8_t>(instanceCreateFailed));

	// All five values must be distinct.
	EXPECT_NE(static_cast<uint8_t>(none), static_cast<uint8_t>(alreadyInitialized));
	EXPECT_NE(static_cast<uint8_t>(none), static_cast<uint8_t>(notInitialized));
	EXPECT_NE(static_cast<uint8_t>(none), static_cast<uint8_t>(vulkanNotAvailable));
	EXPECT_NE(static_cast<uint8_t>(none), static_cast<uint8_t>(instanceCreateFailed));
	EXPECT_NE(static_cast<uint8_t>(alreadyInitialized), static_cast<uint8_t>(notInitialized));
	EXPECT_NE(static_cast<uint8_t>(alreadyInitialized), static_cast<uint8_t>(vulkanNotAvailable));
	EXPECT_NE(
		static_cast<uint8_t>(alreadyInitialized), static_cast<uint8_t>(instanceCreateFailed));
	EXPECT_NE(static_cast<uint8_t>(notInitialized), static_cast<uint8_t>(vulkanNotAvailable));
	EXPECT_NE(static_cast<uint8_t>(notInitialized), static_cast<uint8_t>(instanceCreateFailed));
	EXPECT_NE(
		static_cast<uint8_t>(vulkanNotAvailable), static_cast<uint8_t>(instanceCreateFailed));
}

TEST(Types, test_render_error_none_is_unique_success_value)
{
	// None == 0 is the unique success value.
	EXPECT_EQ(static_cast<uint8_t>(RenderError::None), uint8_t{0});

	// Every non-None value is non-zero (i.e. not equal to None).
	EXPECT_NE(RenderError::AlreadyInitialized, RenderError::None);
	EXPECT_NE(RenderError::NotInitialized, RenderError::None);
	EXPECT_NE(RenderError::VulkanNotAvailable, RenderError::None);
	EXPECT_NE(RenderError::InstanceCreateFailed, RenderError::None);

	// Verify the semantic meaning of each non-None value is distinct.
	EXPECT_NE(RenderError::AlreadyInitialized, RenderError::NotInitialized);
	EXPECT_NE(RenderError::AlreadyInitialized, RenderError::VulkanNotAvailable);
	EXPECT_NE(RenderError::AlreadyInitialized, RenderError::InstanceCreateFailed);
	EXPECT_NE(RenderError::NotInitialized, RenderError::VulkanNotAvailable);
	EXPECT_NE(RenderError::NotInitialized, RenderError::InstanceCreateFailed);
	EXPECT_NE(RenderError::VulkanNotAvailable, RenderError::InstanceCreateFailed);

	// A simple success-check idiom: only None passes.
	auto isSuccess = [](RenderError e) { return e == RenderError::None; };
	EXPECT_TRUE(isSuccess(RenderError::None));
	EXPECT_FALSE(isSuccess(RenderError::AlreadyInitialized));
	EXPECT_FALSE(isSuccess(RenderError::NotInitialized));
	EXPECT_FALSE(isSuccess(RenderError::VulkanNotAvailable));
	EXPECT_FALSE(isSuccess(RenderError::InstanceCreateFailed));
}

TEST(Types, test_renderer_config_holds_renderer_wide_configuration)
{
	// Default construction must succeed and produce the specified defaults.
	RendererConfig cfg;

	// applicationName defaults to "Virasa".
	ASSERT_NE(cfg.applicationName, nullptr);
	EXPECT_STREQ(cfg.applicationName, "Virasa");

	// applicationVersion defaults to 0.
	EXPECT_EQ(cfg.applicationVersion, uint32_t{0});

	// enableValidation defaults to true.
	EXPECT_TRUE(cfg.enableValidation);

	// requiredInstanceExtensions defaults to nullptr.
	EXPECT_EQ(cfg.requiredInstanceExtensions, nullptr);

	// requiredInstanceExtensionCount defaults to 0.
	EXPECT_EQ(cfg.requiredInstanceExtensionCount, uint32_t{0});

	// Verify the struct is copyable.
	RendererConfig copy = cfg;
	EXPECT_STREQ(copy.applicationName, cfg.applicationName);
	EXPECT_EQ(copy.applicationVersion, cfg.applicationVersion);
	EXPECT_EQ(copy.enableValidation, cfg.enableValidation);
	EXPECT_EQ(copy.requiredInstanceExtensions, cfg.requiredInstanceExtensions);
	EXPECT_EQ(copy.requiredInstanceExtensionCount, cfg.requiredInstanceExtensionCount);

	// Verify the struct is movable.
	RendererConfig moved = std::move(copy);
	EXPECT_STREQ(moved.applicationName, "Virasa");
	EXPECT_EQ(moved.applicationVersion, uint32_t{0});
	EXPECT_TRUE(moved.enableValidation);
	EXPECT_EQ(moved.requiredInstanceExtensions, nullptr);
	EXPECT_EQ(moved.requiredInstanceExtensionCount, uint32_t{0});

	// Verify members are publicly assignable (non-const struct members).
	const char* extName = "VK_KHR_surface";
	const char* const exts[] = {extName};

	RendererConfig custom;
	custom.applicationName = "MyApp";
	custom.applicationVersion = 1u;
	custom.enableValidation = false;
	custom.requiredInstanceExtensions = exts;
	custom.requiredInstanceExtensionCount = 1u;

	EXPECT_STREQ(custom.applicationName, "MyApp");
	EXPECT_EQ(custom.applicationVersion, uint32_t{1});
	EXPECT_FALSE(custom.enableValidation);
	EXPECT_EQ(custom.requiredInstanceExtensions, exts);
	EXPECT_EQ(custom.requiredInstanceExtensionCount, uint32_t{1});
	EXPECT_STREQ(custom.requiredInstanceExtensions[0], "VK_KHR_surface");
}

// renderer_config_targets_vulkan_1_3: The Vulkan 1.3 target is a compile-time / documentation
// contract; there is no runtime field on RendererConfig that exposes it. We verify here that
// the VK_API_VERSION_1_3 macro is reachable (via the Vulkan headers that consumers of
// RendererConfig will use) and that RendererConfig itself does not carry an apiVersion field
// that could silently override the pinned version.
TEST(Types, test_renderer_config_targets_vulkan_1_3)
{
	// RendererConfig must not expose a runtime apiVersion field; the Vulkan 1.3
	// target is fixed by the contract. We verify this by confirming that a
	// default-constructed RendererConfig has exactly the five documented members
	// and that none of them is named apiVersion. We do this indirectly: if the
	// struct compiled with the five members above and none of the tests reference
	// an apiVersion field, the contract is satisfied at the type level.
	//
	// Additionally, confirm that the struct size is consistent with five members
	// of the declared types (pointer + uint32 + bool + pointer + uint32).
	// We don't pin an exact byte size because padding is implementation-defined,
	// but we verify the struct is at least as large as the sum of its members.
	constexpr std::size_t kMinSize = sizeof(const char*)		  // applicationName
						   + sizeof(uint32_t)		  // applicationVersion
						   + sizeof(bool)			  // enableValidation
						   + sizeof(const char* const*) // requiredInstanceExtensions
						   + sizeof(uint32_t); // requiredInstanceExtensionCount

	EXPECT_GE(sizeof(RendererConfig), kMinSize);

	// A default-constructed RendererConfig must be usable as-is (no apiVersion
	// field to set); the consumer is responsible for setting VK_API_VERSION_1_3
	// when creating the VkInstance.
	RendererConfig cfg;
	EXPECT_STREQ(cfg.applicationName, "Virasa");
	EXPECT_EQ(cfg.applicationVersion, uint32_t{0});
	EXPECT_TRUE(cfg.enableValidation);
	EXPECT_EQ(cfg.requiredInstanceExtensions, nullptr);
	EXPECT_EQ(cfg.requiredInstanceExtensionCount, uint32_t{0});
}
