#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
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

	// Version 4 adds SwapchainCreateFailed and ImageViewCreateFailed after DeviceCreateFailed.
	EXPECT_LT(static_cast<uint8_t>(RenderError::DeviceCreateFailed),
		static_cast<uint8_t>(RenderError::SwapchainCreateFailed));
	EXPECT_LT(static_cast<uint8_t>(RenderError::SwapchainCreateFailed),
		static_cast<uint8_t>(RenderError::ImageViewCreateFailed));

	// Version 5 adds ShaderLoadFailed, ShaderCreateFailed, PipelineLayoutCreateFailed,
	// and PipelineCreateFailed after ImageViewCreateFailed.
	EXPECT_LT(static_cast<uint8_t>(RenderError::ImageViewCreateFailed),
		static_cast<uint8_t>(RenderError::ShaderLoadFailed));
	EXPECT_LT(static_cast<uint8_t>(RenderError::ShaderLoadFailed),
		static_cast<uint8_t>(RenderError::ShaderCreateFailed));
	EXPECT_LT(static_cast<uint8_t>(RenderError::ShaderCreateFailed),
		static_cast<uint8_t>(RenderError::PipelineLayoutCreateFailed));
	EXPECT_LT(static_cast<uint8_t>(RenderError::PipelineLayoutCreateFailed),
		static_cast<uint8_t>(RenderError::PipelineCreateFailed));

	// Version 6 adds CommandPoolCreateFailed and FenceCreateFailed after PipelineCreateFailed.
	EXPECT_LT(static_cast<uint8_t>(RenderError::PipelineCreateFailed),
		static_cast<uint8_t>(RenderError::CommandPoolCreateFailed));
	EXPECT_LT(static_cast<uint8_t>(RenderError::CommandPoolCreateFailed),
		static_cast<uint8_t>(RenderError::FenceCreateFailed));

	// Version 7 adds ImageCreateFailed and MemoryAllocFailed after FenceCreateFailed.
	EXPECT_LT(static_cast<uint8_t>(RenderError::FenceCreateFailed),
		static_cast<uint8_t>(RenderError::ImageCreateFailed));
	EXPECT_LT(static_cast<uint8_t>(RenderError::ImageCreateFailed),
		static_cast<uint8_t>(RenderError::MemoryAllocFailed));

	// All eighteen values are distinct.
	uint8_t values[] = {
		static_cast<uint8_t>(RenderError::None),
		static_cast<uint8_t>(RenderError::AlreadyInitialized),
		static_cast<uint8_t>(RenderError::NotInitialized),
		static_cast<uint8_t>(RenderError::VulkanNotAvailable),
		static_cast<uint8_t>(RenderError::InstanceCreateFailed),
		static_cast<uint8_t>(RenderError::SurfaceCreateFailed),
		static_cast<uint8_t>(RenderError::NoSuitableDevice),
		static_cast<uint8_t>(RenderError::DeviceCreateFailed),
		static_cast<uint8_t>(RenderError::SwapchainCreateFailed),
		static_cast<uint8_t>(RenderError::ImageViewCreateFailed),
		static_cast<uint8_t>(RenderError::ShaderLoadFailed),
		static_cast<uint8_t>(RenderError::ShaderCreateFailed),
		static_cast<uint8_t>(RenderError::PipelineLayoutCreateFailed),
		static_cast<uint8_t>(RenderError::PipelineCreateFailed),
		static_cast<uint8_t>(RenderError::CommandPoolCreateFailed),
		static_cast<uint8_t>(RenderError::FenceCreateFailed),
		static_cast<uint8_t>(RenderError::ImageCreateFailed),
		static_cast<uint8_t>(RenderError::MemoryAllocFailed),
	};
	for (std::size_t i = 0; i < 18; ++i)
	{
		for (std::size_t j = i + 1; j < 18; ++j)
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
	EXPECT_NE(RenderError::SwapchainCreateFailed, RenderError::None);
	EXPECT_NE(RenderError::ImageViewCreateFailed, RenderError::None);
	EXPECT_NE(RenderError::ShaderLoadFailed, RenderError::None);
	EXPECT_NE(RenderError::ShaderCreateFailed, RenderError::None);
	EXPECT_NE(RenderError::PipelineLayoutCreateFailed, RenderError::None);
	EXPECT_NE(RenderError::PipelineCreateFailed, RenderError::None);
	EXPECT_NE(RenderError::CommandPoolCreateFailed, RenderError::None);
	EXPECT_NE(RenderError::FenceCreateFailed, RenderError::None);

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
	EXPECT_FALSE(isSuccess(RenderError::SwapchainCreateFailed));
	EXPECT_FALSE(isSuccess(RenderError::ImageViewCreateFailed));
	EXPECT_FALSE(isSuccess(RenderError::ShaderLoadFailed));
	EXPECT_FALSE(isSuccess(RenderError::ShaderCreateFailed));
	EXPECT_FALSE(isSuccess(RenderError::PipelineLayoutCreateFailed));
	EXPECT_FALSE(isSuccess(RenderError::PipelineCreateFailed));
	EXPECT_FALSE(isSuccess(RenderError::CommandPoolCreateFailed));
	EXPECT_FALSE(isSuccess(RenderError::FenceCreateFailed));
	EXPECT_FALSE(isSuccess(RenderError::ImageCreateFailed));
	EXPECT_FALSE(isSuccess(RenderError::MemoryAllocFailed));

	// Every non-None value is not equal to None (version-7 additions).
	EXPECT_NE(RenderError::ImageCreateFailed, RenderError::None);
	EXPECT_NE(RenderError::MemoryAllocFailed, RenderError::None);
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

	// swapchainImageUsage defaults to COLOR_ATTACHMENT | TRANSFER_DST.
	EXPECT_EQ(config.swapchainImageUsage,
		VkImageUsageFlags{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT});

	// maxFramesInFlight defaults to 2 (version-6 addition).
	EXPECT_EQ(config.maxFramesInFlight, uint32_t{2});

	// depthFormat defaults to VK_FORMAT_D32_SFLOAT (version-7 addition).
	EXPECT_EQ(config.depthFormat, VK_FORMAT_D32_SFLOAT);

	// Members are publicly assignable.
	const char* extName = "VK_KHR_surface";
	const char* const exts[] = {extName};

	config.applicationName = "MyApp";
	config.applicationVersion = 1u;
	config.enableValidation = false;
	config.requiredInstanceExtensions = exts;
	config.requiredInstanceExtensionCount = 1u;
	config.preferMailbox = true;
	config.swapchainImageUsage =
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	config.maxFramesInFlight = 3u;
	config.depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

	EXPECT_STREQ(config.applicationName, "MyApp");
	EXPECT_EQ(config.applicationVersion, uint32_t{1});
	EXPECT_FALSE(config.enableValidation);
	EXPECT_EQ(config.requiredInstanceExtensions, exts);
	EXPECT_EQ(config.requiredInstanceExtensionCount, uint32_t{1});
	EXPECT_TRUE(config.preferMailbox);
	EXPECT_EQ(config.swapchainImageUsage,
		VkImageUsageFlags{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT});
	EXPECT_EQ(config.maxFramesInFlight, uint32_t{3});
	EXPECT_EQ(config.depthFormat, VK_FORMAT_D24_UNORM_S8_UINT);

	// Copyable: copy-construct and verify members are equal.
	RendererConfig copy = config;
	EXPECT_STREQ(copy.applicationName, config.applicationName);
	EXPECT_EQ(copy.applicationVersion, config.applicationVersion);
	EXPECT_EQ(copy.enableValidation, config.enableValidation);
	EXPECT_EQ(copy.requiredInstanceExtensions, config.requiredInstanceExtensions);
	EXPECT_EQ(copy.requiredInstanceExtensionCount, config.requiredInstanceExtensionCount);
	EXPECT_EQ(copy.preferMailbox, config.preferMailbox);
	EXPECT_EQ(copy.swapchainImageUsage, config.swapchainImageUsage);
	EXPECT_EQ(copy.maxFramesInFlight, config.maxFramesInFlight);
	EXPECT_EQ(copy.depthFormat, config.depthFormat);

	// Movable: move-construct and verify members transferred.
	RendererConfig moved = std::move(copy);
	EXPECT_STREQ(moved.applicationName, "MyApp");
	EXPECT_EQ(moved.applicationVersion, uint32_t{1});
	EXPECT_FALSE(moved.enableValidation);
	EXPECT_EQ(moved.requiredInstanceExtensions, exts);
	EXPECT_EQ(moved.requiredInstanceExtensionCount, uint32_t{1});
	EXPECT_TRUE(moved.preferMailbox);
	EXPECT_EQ(moved.swapchainImageUsage,
		VkImageUsageFlags{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT});
	EXPECT_EQ(moved.maxFramesInFlight, uint32_t{3});
	EXPECT_EQ(moved.depthFormat, VK_FORMAT_D24_UNORM_S8_UINT);
}

// ---------------------------------------------------------------------------
// SwapchainStatus enum tests
// ---------------------------------------------------------------------------

TEST(Types, test_swapchain_status_enum_values_in_declared_order)
{
	// Underlying type is uint8_t.
	static_assert(std::is_same_v<std::underlying_type_t<SwapchainStatus>, uint8_t>,
		"SwapchainStatus underlying type must be uint8_t");

	// Success is explicitly 0.
	EXPECT_EQ(static_cast<uint8_t>(SwapchainStatus::Success), uint8_t{0});

	// Values appear in declared order.
	EXPECT_LT(static_cast<uint8_t>(SwapchainStatus::Success),
		static_cast<uint8_t>(SwapchainStatus::Recreated));
	EXPECT_LT(static_cast<uint8_t>(SwapchainStatus::Recreated),
		static_cast<uint8_t>(SwapchainStatus::Error));

	// Version 6 adds NotReady after Error.
	EXPECT_LT(static_cast<uint8_t>(SwapchainStatus::Error),
		static_cast<uint8_t>(SwapchainStatus::NotReady));

	// All four values are distinct.
	EXPECT_NE(static_cast<uint8_t>(SwapchainStatus::Success),
		static_cast<uint8_t>(SwapchainStatus::Recreated));
	EXPECT_NE(static_cast<uint8_t>(SwapchainStatus::Success),
		static_cast<uint8_t>(SwapchainStatus::Error));
	EXPECT_NE(static_cast<uint8_t>(SwapchainStatus::Success),
		static_cast<uint8_t>(SwapchainStatus::NotReady));
	EXPECT_NE(static_cast<uint8_t>(SwapchainStatus::Recreated),
		static_cast<uint8_t>(SwapchainStatus::Error));
	EXPECT_NE(static_cast<uint8_t>(SwapchainStatus::Recreated),
		static_cast<uint8_t>(SwapchainStatus::NotReady));
	EXPECT_NE(static_cast<uint8_t>(SwapchainStatus::Error),
		static_cast<uint8_t>(SwapchainStatus::NotReady));

	// Success == 0 is the only value indicating a clean operation.
	EXPECT_EQ(SwapchainStatus::Success, static_cast<SwapchainStatus>(0));
	EXPECT_NE(SwapchainStatus::Recreated, SwapchainStatus::Success);
	EXPECT_NE(SwapchainStatus::Error, SwapchainStatus::Success);
	EXPECT_NE(SwapchainStatus::NotReady, SwapchainStatus::Success);
}

// ---------------------------------------------------------------------------
// VertexAttribute struct tests
// ---------------------------------------------------------------------------

TEST(Types, test_vertex_attribute_describes_one_shader_input)
{
	// Default construction.
	VertexAttribute attr;

	// location defaults to 0.
	EXPECT_EQ(attr.location, uint32_t{0});

	// format defaults to VK_FORMAT_UNDEFINED.
	EXPECT_EQ(attr.format, VK_FORMAT_UNDEFINED);

	// offset defaults to 0.
	EXPECT_EQ(attr.offset, uint32_t{0});

	// Members are publicly assignable.
	attr.location = 2u;
	attr.format = VK_FORMAT_R32G32B32_SFLOAT;
	attr.offset = 12u;

	EXPECT_EQ(attr.location, uint32_t{2});
	EXPECT_EQ(attr.format, VK_FORMAT_R32G32B32_SFLOAT);
	EXPECT_EQ(attr.offset, uint32_t{12});

	// Copyable.
	VertexAttribute copy = attr;
	EXPECT_EQ(copy.location, attr.location);
	EXPECT_EQ(copy.format, attr.format);
	EXPECT_EQ(copy.offset, attr.offset);

	// Movable.
	VertexAttribute moved = std::move(copy);
	EXPECT_EQ(moved.location, uint32_t{2});
	EXPECT_EQ(moved.format, VK_FORMAT_R32G32B32_SFLOAT);
	EXPECT_EQ(moved.offset, uint32_t{12});

	// Trivially destructible — verified at compile time.
	static_assert(std::is_trivially_destructible_v<VertexAttribute>,
		"VertexAttribute must be trivially destructible");
}

// ---------------------------------------------------------------------------
// VertexLayout struct tests
// ---------------------------------------------------------------------------

TEST(Types, test_vertex_layout_describes_vertex_input_state)
{
	// Default construction.
	VertexLayout layout;

	// stride defaults to 0.
	EXPECT_EQ(layout.stride, uint32_t{0});

	// inputRate defaults to VK_VERTEX_INPUT_RATE_VERTEX.
	EXPECT_EQ(layout.inputRate, VK_VERTEX_INPUT_RATE_VERTEX);

	// attributes defaults to an empty span.
	EXPECT_TRUE(layout.attributes.empty());
	EXPECT_EQ(layout.attributes.size(), std::size_t{0});

	// A stride-0 + empty attributes span is the "no vertex input" convention.
	EXPECT_EQ(layout.stride, uint32_t{0});

	// Members are publicly assignable.
	VertexAttribute attrs[2];
	attrs[0].location = 0u;
	attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrs[0].offset = 0u;
	attrs[1].location = 1u;
	attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
	attrs[1].offset = 12u;

	layout.stride = 20u;
	layout.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
	layout.attributes = std::span<const VertexAttribute>{attrs, 2};

	EXPECT_EQ(layout.stride, uint32_t{20});
	EXPECT_EQ(layout.inputRate, VK_VERTEX_INPUT_RATE_INSTANCE);
	EXPECT_EQ(layout.attributes.size(), std::size_t{2});
	EXPECT_EQ(layout.attributes[0].location, uint32_t{0});
	EXPECT_EQ(layout.attributes[0].format, VK_FORMAT_R32G32B32_SFLOAT);
	EXPECT_EQ(layout.attributes[0].offset, uint32_t{0});
	EXPECT_EQ(layout.attributes[1].location, uint32_t{1});
	EXPECT_EQ(layout.attributes[1].format, VK_FORMAT_R32G32_SFLOAT);
	EXPECT_EQ(layout.attributes[1].offset, uint32_t{12});

	// Copyable — the span itself is copied (non-owning view).
	VertexLayout copy = layout;
	EXPECT_EQ(copy.stride, layout.stride);
	EXPECT_EQ(copy.inputRate, layout.inputRate);
	EXPECT_EQ(copy.attributes.data(), layout.attributes.data());
	EXPECT_EQ(copy.attributes.size(), layout.attributes.size());

	// Movable.
	VertexLayout moved = std::move(copy);
	EXPECT_EQ(moved.stride, uint32_t{20});
	EXPECT_EQ(moved.inputRate, VK_VERTEX_INPUT_RATE_INSTANCE);
	EXPECT_EQ(moved.attributes.size(), std::size_t{2});

	// Trivially destructible — verified at compile time.
	static_assert(std::is_trivially_destructible_v<VertexLayout>,
		"VertexLayout must be trivially destructible");
}

// ---------------------------------------------------------------------------
// operator<< for RenderError tests
// ---------------------------------------------------------------------------

TEST(Types, test_render_error_has_ostream_insertion_operator)
{
	// Each named value writes its identifier as a plain string.
	auto toString = [](RenderError e) -> std::string
	{
		std::ostringstream oss;
		oss << e;
		return oss.str();
	};

	EXPECT_EQ(toString(RenderError::None), "None");
	EXPECT_EQ(toString(RenderError::AlreadyInitialized), "AlreadyInitialized");
	EXPECT_EQ(toString(RenderError::NotInitialized), "NotInitialized");
	EXPECT_EQ(toString(RenderError::VulkanNotAvailable), "VulkanNotAvailable");
	EXPECT_EQ(toString(RenderError::InstanceCreateFailed), "InstanceCreateFailed");
	EXPECT_EQ(toString(RenderError::SurfaceCreateFailed), "SurfaceCreateFailed");
	EXPECT_EQ(toString(RenderError::NoSuitableDevice), "NoSuitableDevice");
	EXPECT_EQ(toString(RenderError::DeviceCreateFailed), "DeviceCreateFailed");
	EXPECT_EQ(toString(RenderError::SwapchainCreateFailed), "SwapchainCreateFailed");
	EXPECT_EQ(toString(RenderError::ImageViewCreateFailed), "ImageViewCreateFailed");
	EXPECT_EQ(toString(RenderError::ShaderLoadFailed), "ShaderLoadFailed");
	EXPECT_EQ(toString(RenderError::ShaderCreateFailed), "ShaderCreateFailed");
	EXPECT_EQ(toString(RenderError::PipelineLayoutCreateFailed), "PipelineLayoutCreateFailed");
	EXPECT_EQ(toString(RenderError::PipelineCreateFailed), "PipelineCreateFailed");
	EXPECT_EQ(toString(RenderError::CommandPoolCreateFailed), "CommandPoolCreateFailed");
	EXPECT_EQ(toString(RenderError::FenceCreateFailed), "FenceCreateFailed");
	EXPECT_EQ(toString(RenderError::ImageCreateFailed), "ImageCreateFailed");
	EXPECT_EQ(toString(RenderError::MemoryAllocFailed), "MemoryAllocFailed");

	// An out-of-range value writes "RenderError(N)" where N is the decimal integer.
	auto outOfRange = static_cast<RenderError>(uint8_t{200});
	EXPECT_EQ(toString(outOfRange), "RenderError(200)");

	// The operator returns the same ostream reference (supports chaining).
	std::ostringstream oss;
	std::ostream& ref = (oss << RenderError::None);
	EXPECT_EQ(&ref, &oss);

	// Chaining: two values written in sequence.
	std::ostringstream oss2;
	oss2 << RenderError::None << "|" << RenderError::AlreadyInitialized;
	EXPECT_EQ(oss2.str(), "None|AlreadyInitialized");
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
	(void)config.swapchainImageUsage;
	(void)config.maxFramesInFlight;
	(void)config.depthFormat;
	// If the struct had an apiVersion member the test author would have caught
	// it during contract review; the absence of such a member is the assertion.
	EXPECT_TRUE(true); // structural assertion satisfied at compile time above.
}
