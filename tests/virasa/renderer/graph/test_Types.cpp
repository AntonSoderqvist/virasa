#include <gtest/gtest.h>
#include <type_traits>

#include "virasa/renderer/graph/Types.h"

using namespace virasa::renderer::graph;

TEST(Types, test_resource_usage_enum_values_in_declared_order)
{
	using Underlying = std::underlying_type_t<ResourceUsage>;

	static_assert(std::is_same_v<Underlying, uint8_t>);

	EXPECT_EQ(static_cast<Underlying>(ResourceUsage::Undefined), static_cast<Underlying>(0));
	EXPECT_EQ(
		static_cast<Underlying>(ResourceUsage::ColorAttachment), static_cast<Underlying>(1));
	EXPECT_EQ(
		static_cast<Underlying>(ResourceUsage::DepthAttachment), static_cast<Underlying>(2));
	EXPECT_EQ(static_cast<Underlying>(ResourceUsage::DepthReadOnly), static_cast<Underlying>(3));
	EXPECT_EQ(
		static_cast<Underlying>(ResourceUsage::SampledFragment), static_cast<Underlying>(4));
	EXPECT_EQ(static_cast<Underlying>(ResourceUsage::SampledCompute), static_cast<Underlying>(5));
	EXPECT_EQ(
		static_cast<Underlying>(ResourceUsage::StorageReadCompute), static_cast<Underlying>(6));
	EXPECT_EQ(static_cast<Underlying>(ResourceUsage::StorageWriteCompute),
		static_cast<Underlying>(7));
	EXPECT_EQ(static_cast<Underlying>(ResourceUsage::TransferSrc), static_cast<Underlying>(8));
	EXPECT_EQ(static_cast<Underlying>(ResourceUsage::TransferDst), static_cast<Underlying>(9));
	EXPECT_EQ(static_cast<Underlying>(ResourceUsage::Present), static_cast<Underlying>(10));
}

TEST(Types, test_resource_usage_pipeline_barrier_mapping)
{
	// This contract pins the semantic mapping for later graph barrier generation,
	// but this header-only types module exposes only the enum values themselves.
	// Verify the full closed set and stable ordinals that downstream mapping code
	// relies on.
	EXPECT_EQ(static_cast<uint8_t>(ResourceUsage::Undefined), 0);
	EXPECT_EQ(static_cast<uint8_t>(ResourceUsage::ColorAttachment), 1);
	EXPECT_EQ(static_cast<uint8_t>(ResourceUsage::DepthAttachment), 2);
	EXPECT_EQ(static_cast<uint8_t>(ResourceUsage::DepthReadOnly), 3);
	EXPECT_EQ(static_cast<uint8_t>(ResourceUsage::SampledFragment), 4);
	EXPECT_EQ(static_cast<uint8_t>(ResourceUsage::SampledCompute), 5);
	EXPECT_EQ(static_cast<uint8_t>(ResourceUsage::StorageReadCompute), 6);
	EXPECT_EQ(static_cast<uint8_t>(ResourceUsage::StorageWriteCompute), 7);
	EXPECT_EQ(static_cast<uint8_t>(ResourceUsage::TransferSrc), 8);
	EXPECT_EQ(static_cast<uint8_t>(ResourceUsage::TransferDst), 9);
	EXPECT_EQ(static_cast<uint8_t>(ResourceUsage::Present), 10);
}

TEST(Types, test_load_op_enum_values_in_declared_order)
{
	using Underlying = std::underlying_type_t<LoadOp>;

	static_assert(std::is_same_v<Underlying, uint8_t>);

	EXPECT_EQ(static_cast<Underlying>(LoadOp::Load), static_cast<Underlying>(0));
	EXPECT_EQ(static_cast<Underlying>(LoadOp::Clear), static_cast<Underlying>(1));
	EXPECT_EQ(static_cast<Underlying>(LoadOp::DontCare), static_cast<Underlying>(2));
}

TEST(Types, test_store_op_enum_values_in_declared_order)
{
	using Underlying = std::underlying_type_t<StoreOp>;

	static_assert(std::is_same_v<Underlying, uint8_t>);

	EXPECT_EQ(static_cast<Underlying>(StoreOp::Store), static_cast<Underlying>(0));
	EXPECT_EQ(static_cast<Underlying>(StoreOp::DontCare), static_cast<Underlying>(1));
}

TEST(Types, test_image_handle_is_typed_index)
{
	static_assert(std::is_default_constructible_v<ImageHandle>);
	static_assert(std::is_copy_constructible_v<ImageHandle>);
	static_assert(std::is_move_constructible_v<ImageHandle>);
	static_assert(std::is_trivially_destructible_v<ImageHandle>);
	static_assert(!std::is_same_v<ImageHandle, BufferHandle>);
	static_assert(noexcept(std::declval<const ImageHandle&>().IsValid()));

	ImageHandle handle;
	EXPECT_EQ(handle.id, 0xFFFFFFFFu);
	EXPECT_FALSE(handle.IsValid());

	ImageHandle validHandle;
	validHandle.id = 7u;
	EXPECT_TRUE(validHandle.IsValid());

	ImageHandle copied = validHandle;
	EXPECT_EQ(copied.id, 7u);
	EXPECT_TRUE(copied.IsValid());
}

TEST(Types, test_buffer_handle_is_typed_index)
{
	static_assert(std::is_default_constructible_v<BufferHandle>);
	static_assert(std::is_copy_constructible_v<BufferHandle>);
	static_assert(std::is_move_constructible_v<BufferHandle>);
	static_assert(std::is_trivially_destructible_v<BufferHandle>);
	static_assert(!std::is_same_v<BufferHandle, ImageHandle>);
	static_assert(noexcept(std::declval<const BufferHandle&>().IsValid()));

	BufferHandle handle;
	EXPECT_EQ(handle.id, 0xFFFFFFFFu);
	EXPECT_FALSE(handle.IsValid());

	BufferHandle validHandle;
	validHandle.id = 3u;
	EXPECT_TRUE(validHandle.IsValid());

	BufferHandle copied = validHandle;
	EXPECT_EQ(copied.id, 3u);
	EXPECT_TRUE(copied.IsValid());
}

TEST(Types, test_clear_color_describes_rgba_clear)
{
	static_assert(std::is_default_constructible_v<ClearColor>);
	static_assert(std::is_copy_constructible_v<ClearColor>);
	static_assert(std::is_move_constructible_v<ClearColor>);
	static_assert(std::is_trivially_destructible_v<ClearColor>);

	ClearColor color;
	EXPECT_FLOAT_EQ(color.r, 0.0f);
	EXPECT_FLOAT_EQ(color.g, 0.0f);
	EXPECT_FLOAT_EQ(color.b, 0.0f);
	EXPECT_FLOAT_EQ(color.a, 1.0f);

	ClearColor configured;
	configured.r = 0.25f;
	configured.g = 0.5f;
	configured.b = 0.75f;
	configured.a = 0.125f;

	EXPECT_FLOAT_EQ(configured.r, 0.25f);
	EXPECT_FLOAT_EQ(configured.g, 0.5f);
	EXPECT_FLOAT_EQ(configured.b, 0.75f);
	EXPECT_FLOAT_EQ(configured.a, 0.125f);
}

TEST(Types, test_graph_image_desc_describes_transient_image)
{
	static_assert(std::is_default_constructible_v<GraphImageDesc>);
	static_assert(std::is_copy_constructible_v<GraphImageDesc>);
	static_assert(std::is_move_constructible_v<GraphImageDesc>);
	static_assert(std::is_trivially_destructible_v<GraphImageDesc>);
	static_assert(std::is_same_v<decltype(GraphImageDesc{}.width), uint32_t>);
	static_assert(std::is_same_v<decltype(GraphImageDesc{}.height), uint32_t>);
	static_assert(std::is_same_v<decltype(GraphImageDesc{}.format), VkFormat>);
	static_assert(std::is_same_v<decltype(GraphImageDesc{}.usage), VkImageUsageFlags>);
	static_assert(std::is_same_v<decltype(GraphImageDesc{}.aspect), VkImageAspectFlags>);

	GraphImageDesc desc;
	EXPECT_EQ(desc.width, 0u);
	EXPECT_EQ(desc.height, 0u);
	EXPECT_EQ(desc.format, VK_FORMAT_UNDEFINED);
	EXPECT_EQ(desc.usage, 0u);
	EXPECT_EQ(desc.aspect, VK_IMAGE_ASPECT_COLOR_BIT);

	GraphImageDesc configured;
	configured.width = 1920u;
	configured.height = 1080u;
	configured.format = VK_FORMAT_R8G8B8A8_UNORM;
	configured.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	configured.aspect = VK_IMAGE_ASPECT_COLOR_BIT;

	EXPECT_EQ(configured.width, 1920u);
	EXPECT_EQ(configured.height, 1080u);
	EXPECT_EQ(configured.format, VK_FORMAT_R8G8B8A8_UNORM);
	EXPECT_EQ(configured.usage, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	EXPECT_EQ(configured.aspect, VK_IMAGE_ASPECT_COLOR_BIT);
}

TEST(Types, test_graph_buffer_desc_describes_transient_buffer)
{
	static_assert(std::is_default_constructible_v<GraphBufferDesc>);
	static_assert(std::is_copy_constructible_v<GraphBufferDesc>);
	static_assert(std::is_move_constructible_v<GraphBufferDesc>);
	static_assert(std::is_trivially_destructible_v<GraphBufferDesc>);
	static_assert(std::is_same_v<decltype(GraphBufferDesc{}.size), VkDeviceSize>);
	static_assert(std::is_same_v<decltype(GraphBufferDesc{}.usage), VkBufferUsageFlags>);
	static_assert(std::is_same_v<decltype(GraphBufferDesc{}.hostVisible), bool>);

	GraphBufferDesc desc;
	EXPECT_EQ(desc.size, 0u);
	EXPECT_EQ(desc.usage, 0u);
	EXPECT_FALSE(desc.hostVisible);

	GraphBufferDesc configured;
	configured.size = 4096u;
	configured.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	configured.hostVisible = true;

	EXPECT_EQ(configured.size, 4096u);
	EXPECT_EQ(configured.usage,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	EXPECT_TRUE(configured.hostVisible);
}
