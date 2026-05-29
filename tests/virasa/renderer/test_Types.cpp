#include <gtest/gtest.h>

#include "virasa/renderer/Types.h"

#include "vulkan/vulkan.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace virasa;
using namespace virasa::math;

// ---------------------------------------------------------------------------
// RenderError enum tests
// ---------------------------------------------------------------------------

TEST(Types, test_render_error_enum_values_in_declared_order)
{
	static_assert(std::is_same_v<std::underlying_type_t<RenderError>, uint8_t>,
		"RenderError underlying type must be uint8_t");

	constexpr uint8_t values[] = {
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
		static_cast<uint8_t>(RenderError::BufferCreateFailed),
		static_cast<uint8_t>(RenderError::MemoryMapFailed),
		static_cast<uint8_t>(RenderError::SamplerCreateFailed),
		static_cast<uint8_t>(RenderError::DescriptorPoolCreateFailed),
	};

	EXPECT_EQ(values[0], uint8_t{0});
	for (std::size_t i = 1; i < std::size(values); ++i)
	{
		EXPECT_EQ(values[i], static_cast<uint8_t>(i));
		EXPECT_GT(values[i], values[i - 1]);
	}

	for (std::size_t i = 0; i < std::size(values); ++i)
	{
		for (std::size_t j = i + 1; j < std::size(values); ++j)
		{
			EXPECT_NE(values[i], values[j]);
		}
	}
}

TEST(Types, test_render_error_none_is_unique_success_value)
{
	EXPECT_EQ(RenderError::None, static_cast<RenderError>(0));

	auto isSuccess = [](RenderError error)
	{
		return error == RenderError::None;
	};

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
	EXPECT_FALSE(isSuccess(RenderError::BufferCreateFailed));
	EXPECT_FALSE(isSuccess(RenderError::MemoryMapFailed));
	EXPECT_FALSE(isSuccess(RenderError::SamplerCreateFailed));
	EXPECT_FALSE(isSuccess(RenderError::DescriptorPoolCreateFailed));
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
	static_assert(std::is_same_v<std::underlying_type_t<SwapchainStatus>, uint8_t>,
		"SwapchainStatus underlying type must be uint8_t");

	constexpr uint8_t values[] = {
		static_cast<uint8_t>(SwapchainStatus::Success),
		static_cast<uint8_t>(SwapchainStatus::Recreated),
		static_cast<uint8_t>(SwapchainStatus::Error),
		static_cast<uint8_t>(SwapchainStatus::NotReady),
	};

	EXPECT_EQ(values[0], uint8_t{0});
	for (std::size_t i = 1; i < std::size(values); ++i)
	{
		EXPECT_EQ(values[i], static_cast<uint8_t>(i));
		EXPECT_GT(values[i], values[i - 1]);
	}

	for (std::size_t i = 0; i < std::size(values); ++i)
	{
		for (std::size_t j = i + 1; j < std::size(values); ++j)
		{
			EXPECT_NE(values[i], values[j]);
		}
	}
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
	auto toString = [](RenderError error) -> std::string
	{
		std::ostringstream oss;
		oss << error;
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
	EXPECT_EQ(toString(RenderError::BufferCreateFailed), "BufferCreateFailed");
	EXPECT_EQ(toString(RenderError::MemoryMapFailed), "MemoryMapFailed");
	EXPECT_EQ(toString(RenderError::SamplerCreateFailed), "SamplerCreateFailed");
	EXPECT_EQ(toString(RenderError::DescriptorPoolCreateFailed), "DescriptorPoolCreateFailed");

	// Out-of-range value: written as "RenderError(N)" where N is the decimal integer.
	EXPECT_EQ(toString(static_cast<RenderError>(uint8_t{200})), "RenderError(200)");
	// Value 255 is also out of range.
	EXPECT_EQ(toString(static_cast<RenderError>(uint8_t{255})), "RenderError(255)");

	// Returns the same ostream reference (chaining support).
	std::ostringstream oss;
	std::ostream& ref = (oss << RenderError::None);
	EXPECT_EQ(&ref, &oss);

	// Chaining works correctly.
	std::ostringstream chained;
	chained << RenderError::None << "|" << RenderError::SamplerCreateFailed;
	EXPECT_EQ(chained.str(), "None|SamplerCreateFailed");

	// No surrounding whitespace or punctuation.
	EXPECT_EQ(toString(RenderError::DescriptorPoolCreateFailed), "DescriptorPoolCreateFailed");
}

TEST(Types, test_renderer_config_targets_vulkan_1_3)
{
	constexpr uint32_t expectedApiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0);
	EXPECT_EQ(VK_API_VERSION_1_3, expectedApiVersion);

	static_assert(std::is_default_constructible_v<RendererConfig>,
		"RendererConfig must be default-constructible");
	static_assert(std::is_copy_constructible_v<RendererConfig>,
		"RendererConfig must be copy-constructible");
	static_assert(std::is_move_constructible_v<RendererConfig>,
		"RendererConfig must be move-constructible");

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

	EXPECT_EQ(VK_VERSION_MAJOR(VK_API_VERSION_1_3), 1u);
	EXPECT_EQ(VK_VERSION_MINOR(VK_API_VERSION_1_3), 3u);
}

TEST(Types, test_camera_domain_enum_values_in_declared_order)
{
	static_assert(std::is_same_v<std::underlying_type_t<CameraDomain>, uint8_t>,
		"CameraDomain underlying type must be uint8_t");

	constexpr uint8_t values[] = {
		static_cast<uint8_t>(CameraDomain::Main),
		static_cast<uint8_t>(CameraDomain::Editor),
	};

	EXPECT_EQ(values[0], uint8_t{0});
	EXPECT_EQ(values[1], uint8_t{1});
	EXPECT_LT(values[0], values[1]);
	EXPECT_NE(values[0], values[1]);
}

TEST(Types, test_vertex_struct_layout)
{
	static_assert(std::is_default_constructible_v<Vertex>,
		"Vertex must be default-constructible");
	static_assert(std::is_copy_constructible_v<Vertex>,
		"Vertex must be copy-constructible");
	static_assert(std::is_move_constructible_v<Vertex>,
		"Vertex must be move-constructible");
	static_assert(std::is_same_v<decltype(Vertex::position), Vec3>,
		"Vertex::position must be virasa::math::Vec3");
	static_assert(std::is_same_v<decltype(Vertex::normal), Vec3>,
		"Vertex::normal must be virasa::math::Vec3");
	static_assert(std::is_same_v<decltype(Vertex::tangent), Vec4>,
		"Vertex::tangent must be virasa::math::Vec4");
	static_assert(std::is_same_v<decltype(Vertex::uv), Vec2>,
		"Vertex::uv must be virasa::math::Vec2");
	static_assert(offsetof(Vertex, position) == 0,
		"Vertex::position must be at byte offset 0");
	static_assert(offsetof(Vertex, normal) == 12,
		"Vertex::normal must be at byte offset 12");
	static_assert(offsetof(Vertex, tangent) == 24,
		"Vertex::tangent must be at byte offset 24");
	static_assert(offsetof(Vertex, uv) == 40,
		"Vertex::uv must be at byte offset 40");
	static_assert(sizeof(Vertex) == 48,
		"Vertex must be tightly packed to 48 bytes");

	Vertex vertex{};
	EXPECT_FLOAT_EQ(vertex.position.x, 0.0f);
	EXPECT_FLOAT_EQ(vertex.position.y, 0.0f);
	EXPECT_FLOAT_EQ(vertex.position.z, 0.0f);
	EXPECT_FLOAT_EQ(vertex.normal.x, 0.0f);
	EXPECT_FLOAT_EQ(vertex.normal.y, 0.0f);
	EXPECT_FLOAT_EQ(vertex.normal.z, 0.0f);
	EXPECT_FLOAT_EQ(vertex.tangent.x, 0.0f);
	EXPECT_FLOAT_EQ(vertex.tangent.y, 0.0f);
	EXPECT_FLOAT_EQ(vertex.tangent.z, 0.0f);
	EXPECT_FLOAT_EQ(vertex.tangent.w, 0.0f);
	EXPECT_FLOAT_EQ(vertex.uv.x, 0.0f);
	EXPECT_FLOAT_EQ(vertex.uv.y, 0.0f);

	vertex.position = Vec3{1.0f, 2.0f, 3.0f};
	vertex.normal = Vec3{4.0f, 5.0f, 6.0f};
	vertex.tangent = Vec4{7.0f, 8.0f, 9.0f, -1.0f};
	vertex.uv = Vec2{0.25f, 0.75f};

	EXPECT_FLOAT_EQ(vertex.position.x, 1.0f);
	EXPECT_FLOAT_EQ(vertex.position.y, 2.0f);
	EXPECT_FLOAT_EQ(vertex.position.z, 3.0f);
	EXPECT_FLOAT_EQ(vertex.normal.x, 4.0f);
	EXPECT_FLOAT_EQ(vertex.normal.y, 5.0f);
	EXPECT_FLOAT_EQ(vertex.normal.z, 6.0f);
	EXPECT_FLOAT_EQ(vertex.tangent.x, 7.0f);
	EXPECT_FLOAT_EQ(vertex.tangent.y, 8.0f);
	EXPECT_FLOAT_EQ(vertex.tangent.z, 9.0f);
	EXPECT_FLOAT_EQ(vertex.tangent.w, -1.0f);
	EXPECT_FLOAT_EQ(vertex.uv.x, 0.25f);
	EXPECT_FLOAT_EQ(vertex.uv.y, 0.75f);
}

TEST(Types, test_mesh_data_holds_cpu_geometry)
{
	static_assert(std::is_default_constructible_v<MeshData>,
		"MeshData must be default-constructible");
	static_assert(std::is_copy_constructible_v<MeshData>,
		"MeshData must be copy-constructible");
	static_assert(std::is_move_constructible_v<MeshData>,
		"MeshData must be move-constructible");

	MeshData mesh;
	EXPECT_TRUE(mesh.vertices.empty());
	EXPECT_TRUE(mesh.indices.empty());
	EXPECT_EQ(mesh.vertices.size(), std::size_t{0});
	EXPECT_EQ(mesh.indices.size(), std::size_t{0});

	Vertex v0{};
	v0.position = glm::vec3{1.0f, 0.0f, 0.0f};
	v0.normal = glm::vec3{0.0f, 1.0f, 0.0f};
	v0.tangent = glm::vec4{1.0f, 0.0f, 0.0f, 1.0f};
	v0.uv = glm::vec2{0.0f, 0.0f};

	Vertex v1{};
	v1.position = glm::vec3{0.0f, 1.0f, 0.0f};
	v1.normal = glm::vec3{0.0f, 1.0f, 0.0f};
	v1.tangent = glm::vec4{0.0f, 1.0f, 0.0f, -1.0f};
	v1.uv = glm::vec2{1.0f, 0.0f};

	mesh.vertices.push_back(v0);
	mesh.vertices.push_back(v1);
	mesh.indices.push_back(0u);
	mesh.indices.push_back(1u);
	mesh.indices.push_back(0u);

	EXPECT_EQ(mesh.vertices.size(), std::size_t{2});
	EXPECT_EQ(mesh.indices.size(), std::size_t{3});
	EXPECT_FLOAT_EQ(mesh.vertices[0].position.x, 1.0f);
	EXPECT_FLOAT_EQ(mesh.vertices[1].position.y, 1.0f);
	EXPECT_FLOAT_EQ(mesh.vertices[0].tangent.w, 1.0f);
	EXPECT_FLOAT_EQ(mesh.vertices[1].tangent.w, -1.0f);
	EXPECT_EQ(mesh.indices[0], uint32_t{0});
	EXPECT_EQ(mesh.indices[1], uint32_t{1});
	EXPECT_EQ(mesh.indices[2], uint32_t{0});

	MeshData copy = mesh;
	ASSERT_EQ(copy.vertices.size(), mesh.vertices.size());
	ASSERT_EQ(copy.indices.size(), mesh.indices.size());
	EXPECT_NE(copy.vertices.data(), mesh.vertices.data());
	EXPECT_NE(copy.indices.data(), mesh.indices.data());
	EXPECT_FLOAT_EQ(copy.vertices[0].position.x, mesh.vertices[0].position.x);
	EXPECT_FLOAT_EQ(copy.vertices[1].uv.x, mesh.vertices[1].uv.x);
	EXPECT_FLOAT_EQ(copy.vertices[0].tangent.w, mesh.vertices[0].tangent.w);
	EXPECT_EQ(copy.indices[0], mesh.indices[0]);
	EXPECT_EQ(copy.indices[1], mesh.indices[1]);
	EXPECT_EQ(copy.indices[2], mesh.indices[2]);

	MeshData moved = std::move(mesh);
	EXPECT_EQ(moved.vertices.size(), std::size_t{2});
	EXPECT_EQ(moved.indices.size(), std::size_t{3});
	EXPECT_FLOAT_EQ(moved.vertices[0].normal.y, 1.0f);
	EXPECT_FLOAT_EQ(moved.vertices[1].uv.x, 1.0f);
	EXPECT_FLOAT_EQ(moved.vertices[1].tangent.y, 1.0f);
	EXPECT_EQ(moved.indices[0], uint32_t{0});
	EXPECT_EQ(moved.indices[1], uint32_t{1});
	EXPECT_EQ(moved.indices[2], uint32_t{0});
}

TEST(Types, test_register_error_enum_values_in_declared_order)
{
	static_assert(std::is_same_v<std::underlying_type_t<RegisterError>, uint8_t>,
		"RegisterError underlying type must be uint8_t");

	constexpr uint8_t values[] = {
		static_cast<uint8_t>(RegisterError::None),
		static_cast<uint8_t>(RegisterError::OutOfSlots),
		static_cast<uint8_t>(RegisterError::InvalidInput),
		static_cast<uint8_t>(RegisterError::UploadFailed),
		static_cast<uint8_t>(RegisterError::SamplerCreateFailed),
	};

	EXPECT_EQ(values[0], uint8_t{0});
	for (std::size_t i = 1; i < std::size(values); ++i)
	{
		EXPECT_EQ(values[i], static_cast<uint8_t>(i));
		EXPECT_GT(values[i], values[i - 1]);
	}

	for (std::size_t i = 0; i < std::size(values); ++i)
	{
		for (std::size_t j = i + 1; j < std::size(values); ++j)
		{
			EXPECT_NE(values[i], values[j]);
		}
	}
}

TEST(Types, test_sampler_config_describes_vk_sampler_parameters)
{
	static_assert(std::is_default_constructible_v<SamplerConfig>,
		"SamplerConfig must be default-constructible");
	static_assert(std::is_copy_constructible_v<SamplerConfig>,
		"SamplerConfig must be copy-constructible");
	static_assert(std::is_move_constructible_v<SamplerConfig>,
		"SamplerConfig must be move-constructible");
	static_assert(std::is_trivially_destructible_v<SamplerConfig>,
		"SamplerConfig must be trivially destructible");
	static_assert(std::is_same_v<decltype(SamplerConfig::magFilter), VkFilter>,
		"magFilter must be VkFilter");
	static_assert(std::is_same_v<decltype(SamplerConfig::minFilter), VkFilter>,
		"minFilter must be VkFilter");
	static_assert(std::is_same_v<decltype(SamplerConfig::mipmapMode), VkSamplerMipmapMode>,
		"mipmapMode must be VkSamplerMipmapMode");
	static_assert(std::is_same_v<decltype(SamplerConfig::addressModeU), VkSamplerAddressMode>,
		"addressModeU must be VkSamplerAddressMode");
	static_assert(std::is_same_v<decltype(SamplerConfig::addressModeV), VkSamplerAddressMode>,
		"addressModeV must be VkSamplerAddressMode");
	static_assert(std::is_same_v<decltype(SamplerConfig::addressModeW), VkSamplerAddressMode>,
		"addressModeW must be VkSamplerAddressMode");
	static_assert(std::is_same_v<decltype(SamplerConfig::anisotropyEnable), bool>,
		"anisotropyEnable must be bool");
	static_assert(std::is_same_v<decltype(SamplerConfig::maxAnisotropy), float>,
		"maxAnisotropy must be float");

	SamplerConfig config;
	EXPECT_EQ(config.magFilter, VK_FILTER_LINEAR);
	EXPECT_EQ(config.minFilter, VK_FILTER_LINEAR);
	EXPECT_EQ(config.mipmapMode, VK_SAMPLER_MIPMAP_MODE_LINEAR);
	EXPECT_EQ(config.addressModeU, VK_SAMPLER_ADDRESS_MODE_REPEAT);
	EXPECT_EQ(config.addressModeV, VK_SAMPLER_ADDRESS_MODE_REPEAT);
	EXPECT_EQ(config.addressModeW, VK_SAMPLER_ADDRESS_MODE_REPEAT);
	EXPECT_FALSE(config.anisotropyEnable);
	EXPECT_FLOAT_EQ(config.maxAnisotropy, 1.0f);

	SamplerConfig sameDefaults;
	EXPECT_TRUE(config == sameDefaults);

	SamplerConfig different = config;
	different.magFilter = VK_FILTER_NEAREST;
	EXPECT_FALSE(config == different);
	different = config;
	different.minFilter = VK_FILTER_NEAREST;
	EXPECT_FALSE(config == different);
	different = config;
	different.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	EXPECT_FALSE(config == different);
	different = config;
	different.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	EXPECT_FALSE(config == different);
	different = config;
	different.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	EXPECT_FALSE(config == different);
	different = config;
	different.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	EXPECT_FALSE(config == different);
	different = config;
	different.anisotropyEnable = true;
	EXPECT_FALSE(config == different);
	different = config;
	different.maxAnisotropy = 16.0f;
	EXPECT_FALSE(config == different);

	SamplerConfig anisotropyDisabledA{};
	SamplerConfig anisotropyDisabledB{};
	anisotropyDisabledA.anisotropyEnable = false;
	anisotropyDisabledB.anisotropyEnable = false;
	anisotropyDisabledA.maxAnisotropy = 1.0f;
	anisotropyDisabledB.maxAnisotropy = 8.0f;
	EXPECT_FALSE(anisotropyDisabledA == anisotropyDisabledB);

	SamplerConfig customized{};
	customized.magFilter = VK_FILTER_NEAREST;
	customized.minFilter = VK_FILTER_NEAREST;
	customized.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	customized.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	customized.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	customized.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	customized.anisotropyEnable = true;
	customized.maxAnisotropy = 4.0f;

	SamplerConfig copy = customized;
	EXPECT_TRUE(copy == customized);

	SamplerConfig moved = std::move(copy);
	EXPECT_TRUE(moved == customized);
}

TEST(Types, test_texture_upload_describes_one_sampled_texture_registration)
{
	static_assert(std::is_default_constructible_v<TextureUpload>,
		"TextureUpload must be default-constructible");
	static_assert(std::is_copy_constructible_v<TextureUpload>,
		"TextureUpload must be copy-constructible");
	static_assert(std::is_move_constructible_v<TextureUpload>,
		"TextureUpload must be move-constructible");
	static_assert(std::is_trivially_destructible_v<TextureUpload>,
		"TextureUpload must be trivially destructible");
	static_assert(std::is_same_v<decltype(TextureUpload::pixels), std::span<const std::byte>>,
		"pixels must be std::span<const std::byte>");
	static_assert(std::is_same_v<decltype(TextureUpload::width), uint32_t>,
		"width must be uint32_t");
	static_assert(std::is_same_v<decltype(TextureUpload::height), uint32_t>,
		"height must be uint32_t");
	static_assert(std::is_same_v<decltype(TextureUpload::format), VkFormat>,
		"format must be VkFormat");
	static_assert(std::is_same_v<decltype(TextureUpload::sampler), SamplerConfig>,
		"sampler must be SamplerConfig");
	static_assert(offsetof(TextureUpload, pixels) == 0,
		"pixels must be the first member");
	static_assert(offsetof(TextureUpload, width) > offsetof(TextureUpload, pixels),
		"width must follow pixels");
	static_assert(offsetof(TextureUpload, height) > offsetof(TextureUpload, width),
		"height must follow width");
	static_assert(offsetof(TextureUpload, format) > offsetof(TextureUpload, height),
		"format must follow height");
	static_assert(offsetof(TextureUpload, sampler) > offsetof(TextureUpload, format),
		"sampler must follow format");

	TextureUpload upload;
	EXPECT_TRUE(upload.pixels.empty());
	EXPECT_EQ(upload.width, 0u);
	EXPECT_EQ(upload.height, 0u);
	EXPECT_EQ(upload.format, VK_FORMAT_R8G8B8A8_UNORM);
	EXPECT_TRUE(upload.sampler == SamplerConfig{});

	const std::array<std::byte, 16> pixels = {
		std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04},
		std::byte{0x05}, std::byte{0x06}, std::byte{0x07}, std::byte{0x08},
		std::byte{0x09}, std::byte{0x0A}, std::byte{0x0B}, std::byte{0x0C},
		std::byte{0x0D}, std::byte{0x0E}, std::byte{0x0F}, std::byte{0x10},
	};
	SamplerConfig sampler{};
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.anisotropyEnable = true;
	sampler.maxAnisotropy = 8.0f;

	upload.pixels = std::span<const std::byte>{pixels};
	upload.width = 2u;
	upload.height = 2u;
	upload.format = VK_FORMAT_R8G8B8A8_SRGB;
	upload.sampler = sampler;

	EXPECT_EQ(upload.pixels.data(), pixels.data());
	EXPECT_EQ(upload.pixels.size(), pixels.size());
	EXPECT_EQ(upload.width, 2u);
	EXPECT_EQ(upload.height, 2u);
	EXPECT_EQ(upload.format, VK_FORMAT_R8G8B8A8_SRGB);
	EXPECT_TRUE(upload.sampler == sampler);

	TextureUpload copy = upload;
	EXPECT_EQ(copy.pixels.data(), upload.pixels.data());
	EXPECT_EQ(copy.pixels.size(), upload.pixels.size());
	EXPECT_EQ(copy.width, upload.width);
	EXPECT_EQ(copy.height, upload.height);
	EXPECT_EQ(copy.format, upload.format);
	EXPECT_TRUE(copy.sampler == upload.sampler);

	TextureUpload moved = std::move(copy);
	EXPECT_EQ(moved.pixels.data(), pixels.data());
	EXPECT_EQ(moved.pixels.size(), pixels.size());
	EXPECT_EQ(moved.width, 2u);
	EXPECT_EQ(moved.height, 2u);
	EXPECT_EQ(moved.format, VK_FORMAT_R8G8B8A8_SRGB);
	EXPECT_TRUE(moved.sampler == sampler);
}
