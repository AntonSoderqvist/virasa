#include <gtest/gtest.h>

#include "virasa/renderer/graph/Pass.h"
#include "virasa/renderer/graph/Types.h"

#include <array>
#include <concepts>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace virasa::renderer::graph;

namespace
{

template <typename T>
concept HasGetReads = requires(const T& value)
{
	value.GetReads();
};

template <typename T>
concept HasGetWrites = requires(const T& value)
{
	value.GetWrites();
};

template <typename T>
concept HasImageReadGetter = requires(const T& value)
{
	value.GetImageReads();
};

template <typename T>
concept HasImageWriteGetter = requires(const T& value)
{
	value.GetImageWrites();
};

template <typename T>
concept HasBufferReadGetter = requires(const T& value)
{
	value.GetBufferReads();
};

template <typename T>
concept HasBufferWriteGetter = requires(const T& value)
{
	value.GetBufferWrites();
};

} // namespace

TEST(Pass, test_color_attachment_info_describes_pass_color_target)
{
	static_assert(std::is_standard_layout_v<ColorAttachmentInfo>);
	static_assert(std::is_trivially_destructible_v<ColorAttachmentInfo>);
	static_assert(std::is_copy_constructible_v<ColorAttachmentInfo>);
	static_assert(std::is_copy_assignable_v<ColorAttachmentInfo>);
	static_assert(std::is_move_constructible_v<ColorAttachmentInfo>);
	static_assert(std::is_move_assignable_v<ColorAttachmentInfo>);
	static_assert(std::is_default_constructible_v<ColorAttachmentInfo>);

	ColorAttachmentInfo info{};

	EXPECT_FALSE(info.image.IsValid());
	EXPECT_EQ(info.loadOp, LoadOp::DontCare);
	EXPECT_EQ(info.storeOp, StoreOp::Store);
	EXPECT_FLOAT_EQ(info.clearColor.r, 0.0f);
	EXPECT_FLOAT_EQ(info.clearColor.g, 0.0f);
	EXPECT_FLOAT_EQ(info.clearColor.b, 0.0f);
	EXPECT_FLOAT_EQ(info.clearColor.a, 1.0f);

	ColorAttachmentInfo populated{};
	populated.image = ImageHandle{7u};
	populated.loadOp = LoadOp::Clear;
	populated.storeOp = StoreOp::Store;
	populated.clearColor = ClearColor{0.25f, 0.5f, 0.75f, 1.0f};

	EXPECT_TRUE(populated.image.IsValid());
	EXPECT_EQ(populated.image.id, 7u);
	EXPECT_EQ(populated.loadOp, LoadOp::Clear);
	EXPECT_EQ(populated.storeOp, StoreOp::Store);
	EXPECT_FLOAT_EQ(populated.clearColor.r, 0.25f);
	EXPECT_FLOAT_EQ(populated.clearColor.g, 0.5f);
	EXPECT_FLOAT_EQ(populated.clearColor.b, 0.75f);
	EXPECT_FLOAT_EQ(populated.clearColor.a, 1.0f);

	EXPECT_LT(offsetof(ColorAttachmentInfo, image), offsetof(ColorAttachmentInfo, loadOp));
	EXPECT_LT(offsetof(ColorAttachmentInfo, loadOp), offsetof(ColorAttachmentInfo, storeOp));
	EXPECT_LT(offsetof(ColorAttachmentInfo, storeOp), offsetof(ColorAttachmentInfo, clearColor));
}

TEST(Pass, test_depth_attachment_info_describes_pass_depth_target)
{
	static_assert(std::is_standard_layout_v<DepthAttachmentInfo>);
	static_assert(std::is_trivially_destructible_v<DepthAttachmentInfo>);
	static_assert(std::is_copy_constructible_v<DepthAttachmentInfo>);
	static_assert(std::is_copy_assignable_v<DepthAttachmentInfo>);
	static_assert(std::is_move_constructible_v<DepthAttachmentInfo>);
	static_assert(std::is_move_assignable_v<DepthAttachmentInfo>);
	static_assert(std::is_default_constructible_v<DepthAttachmentInfo>);

	DepthAttachmentInfo info{};

	EXPECT_FALSE(info.image.IsValid());
	EXPECT_EQ(info.loadOp, LoadOp::DontCare);
	EXPECT_EQ(info.storeOp, StoreOp::DontCare);
	EXPECT_FLOAT_EQ(info.clearDepth, 1.0f);
	EXPECT_FALSE(info.present);

	DepthAttachmentInfo populated{};
	populated.image = ImageHandle{9u};
	populated.loadOp = LoadOp::Clear;
	populated.storeOp = StoreOp::Store;
	populated.clearDepth = 0.125f;
	populated.present = true;

	EXPECT_TRUE(populated.image.IsValid());
	EXPECT_EQ(populated.image.id, 9u);
	EXPECT_EQ(populated.loadOp, LoadOp::Clear);
	EXPECT_EQ(populated.storeOp, StoreOp::Store);
	EXPECT_FLOAT_EQ(populated.clearDepth, 0.125f);
	EXPECT_TRUE(populated.present);

	EXPECT_LT(offsetof(DepthAttachmentInfo, image), offsetof(DepthAttachmentInfo, loadOp));
	EXPECT_LT(offsetof(DepthAttachmentInfo, loadOp), offsetof(DepthAttachmentInfo, storeOp));
	EXPECT_LT(offsetof(DepthAttachmentInfo, storeOp), offsetof(DepthAttachmentInfo, clearDepth));
	EXPECT_LT(offsetof(DepthAttachmentInfo, clearDepth), offsetof(DepthAttachmentInfo, present));
}

TEST(Pass, test_image_access_describes_image_resource_use)
{
	static_assert(std::is_standard_layout_v<ImageAccess>);
	static_assert(std::is_trivially_destructible_v<ImageAccess>);
	static_assert(std::is_copy_constructible_v<ImageAccess>);
	static_assert(std::is_copy_assignable_v<ImageAccess>);
	static_assert(std::is_move_constructible_v<ImageAccess>);
	static_assert(std::is_move_assignable_v<ImageAccess>);
	static_assert(std::is_default_constructible_v<ImageAccess>);

	ImageAccess access{};

	EXPECT_FALSE(access.image.IsValid());
	EXPECT_EQ(access.usage, ResourceUsage::Undefined);

	ImageAccess populated{};
	populated.image = ImageHandle{3u};
	populated.usage = ResourceUsage::SampledFragment;

	EXPECT_TRUE(populated.image.IsValid());
	EXPECT_EQ(populated.image.id, 3u);
	EXPECT_EQ(populated.usage, ResourceUsage::SampledFragment);
}

TEST(Pass, test_buffer_access_describes_buffer_resource_use)
{
	static_assert(std::is_standard_layout_v<BufferAccess>);
	static_assert(std::is_trivially_destructible_v<BufferAccess>);
	static_assert(std::is_copy_constructible_v<BufferAccess>);
	static_assert(std::is_copy_assignable_v<BufferAccess>);
	static_assert(std::is_move_constructible_v<BufferAccess>);
	static_assert(std::is_move_assignable_v<BufferAccess>);
	static_assert(std::is_default_constructible_v<BufferAccess>);

	BufferAccess access{};

	EXPECT_FALSE(access.buffer.IsValid());
	EXPECT_EQ(access.usage, ResourceUsage::Undefined);

	BufferAccess populated{};
	populated.buffer = BufferHandle{5u};
	populated.usage = ResourceUsage::StorageWriteCompute;

	EXPECT_TRUE(populated.buffer.IsValid());
	EXPECT_EQ(populated.buffer.id, 5u);
	EXPECT_EQ(populated.usage, ResourceUsage::StorageWriteCompute);
}

TEST(Pass, test_graph_context_is_record_time_view)
{
	static_assert(std::is_final_v<GraphContext>);
	static_assert(!std::is_default_constructible_v<GraphContext>);
	static_assert(!std::is_copy_constructible_v<GraphContext>);
	static_assert(!std::is_copy_assignable_v<GraphContext>);
	static_assert(!std::is_move_constructible_v<GraphContext>);
	static_assert(!std::is_move_assignable_v<GraphContext>);

	static_assert(noexcept(std::declval<const GraphContext&>().GetCommandBuffer()));
	static_assert(noexcept(std::declval<const GraphContext&>().GetRenderExtent()));
	static_assert(noexcept(std::declval<const GraphContext&>().GetImageView(std::declval<ImageHandle>())));
	static_assert(noexcept(std::declval<const GraphContext&>().GetImage(std::declval<ImageHandle>())));
	static_assert(noexcept(std::declval<const GraphContext&>().GetBuffer(std::declval<BufferHandle>())));
	static_assert(noexcept(std::declval<const GraphContext&>().GetBufferAddress(std::declval<BufferHandle>())));

	SUCCEED();
}

TEST(Pass, test_graph_context_command_buffer_observer)
{
	static_assert(std::same_as<decltype(std::declval<const GraphContext&>().GetCommandBuffer()),
		VkCommandBuffer>);
	static_assert(noexcept(std::declval<const GraphContext&>().GetCommandBuffer()));

	SUCCEED();
}

TEST(Pass, test_graph_context_render_extent_observer)
{
	static_assert(std::same_as<decltype(std::declval<const GraphContext&>().GetRenderExtent()),
		VkExtent2D>);
	static_assert(noexcept(std::declval<const GraphContext&>().GetRenderExtent()));

	SUCCEED();
}

TEST(Pass, test_graph_context_image_observers)
{
	static_assert(std::same_as<decltype(std::declval<const GraphContext&>().GetImageView(std::declval<ImageHandle>())),
		VkImageView>);
	static_assert(std::same_as<decltype(std::declval<const GraphContext&>().GetImage(std::declval<ImageHandle>())),
		VkImage>);
	static_assert(noexcept(std::declval<const GraphContext&>().GetImageView(std::declval<ImageHandle>())));
	static_assert(noexcept(std::declval<const GraphContext&>().GetImage(std::declval<ImageHandle>())));

	SUCCEED();
}

TEST(Pass, test_graph_context_buffer_observers)
{
	static_assert(std::same_as<decltype(std::declval<const GraphContext&>().GetBuffer(std::declval<BufferHandle>())),
		VkBuffer>);
	static_assert(std::same_as<decltype(std::declval<const GraphContext&>().GetBufferAddress(std::declval<BufferHandle>())),
		VkDeviceAddress>);
	static_assert(noexcept(std::declval<const GraphContext&>().GetBuffer(std::declval<BufferHandle>())));
	static_assert(noexcept(std::declval<const GraphContext&>().GetBufferAddress(std::declval<BufferHandle>())));

	SUCCEED();
}

TEST(Pass, test_pass_holds_declarative_state)
{
	static_assert(std::is_final_v<Pass>);
	static_assert(std::is_default_constructible_v<Pass>);
	static_assert(!std::is_copy_constructible_v<Pass>);
	static_assert(!std::is_copy_assignable_v<Pass>);
	static_assert(std::is_move_constructible_v<Pass>);
	static_assert(std::is_move_assignable_v<Pass>);

	static_assert(noexcept(std::declval<const Pass&>().GetName()));
	static_assert(noexcept(std::declval<const Pass&>().GetColorAttachments()));
	static_assert(noexcept(std::declval<const Pass&>().HasDepthAttachment()));
	static_assert(noexcept(std::declval<const Pass&>().GetDepthAttachment()));
	static_assert(noexcept(std::declval<const Pass&>().GetImageAccesses()));
	static_assert(noexcept(std::declval<const Pass&>().GetBufferAccesses()));
	static_assert(noexcept(std::declval<const Pass&>().HasRecordCallback()));
	static_assert(!noexcept(std::declval<const Pass&>().Invoke(std::declval<const GraphContext&>())));

	static_assert(std::same_as<decltype(std::declval<const Pass&>().GetName()), const char*>);
	static_assert(std::same_as<decltype(std::declval<const Pass&>().GetColorAttachments()),
		std::span<const ColorAttachmentInfo>>);
	static_assert(std::same_as<decltype(std::declval<const Pass&>().HasDepthAttachment()), bool>);
	static_assert(std::same_as<decltype(std::declval<const Pass&>().GetDepthAttachment()),
		const DepthAttachmentInfo&>);
	static_assert(std::same_as<decltype(std::declval<const Pass&>().GetImageAccesses()),
		std::span<const ImageAccess>>);
	static_assert(std::same_as<decltype(std::declval<const Pass&>().GetBufferAccesses()),
		std::span<const BufferAccess>>);
	static_assert(std::same_as<decltype(std::declval<const Pass&>().HasRecordCallback()), bool>);

	Pass pass;

	EXPECT_EQ(pass.GetName(), nullptr);
	EXPECT_TRUE(pass.GetColorAttachments().empty());
	EXPECT_FALSE(pass.HasDepthAttachment());
	EXPECT_FALSE(pass.GetDepthAttachment().present);
	EXPECT_FALSE(pass.GetDepthAttachment().image.IsValid());
	EXPECT_TRUE(pass.GetImageAccesses().empty());
	EXPECT_TRUE(pass.GetBufferAccesses().empty());
	EXPECT_FALSE(pass.HasRecordCallback());

	Pass movedTo = std::move(pass);

	EXPECT_EQ(pass.GetName(), nullptr);
	EXPECT_TRUE(pass.GetColorAttachments().empty());
	EXPECT_FALSE(pass.HasDepthAttachment());
	EXPECT_TRUE(pass.GetImageAccesses().empty());
	EXPECT_TRUE(pass.GetBufferAccesses().empty());
	EXPECT_FALSE(pass.HasRecordCallback());

	EXPECT_EQ(movedTo.GetName(), nullptr);
	EXPECT_TRUE(movedTo.GetColorAttachments().empty());
	EXPECT_FALSE(movedTo.HasDepthAttachment());
	EXPECT_TRUE(movedTo.GetImageAccesses().empty());
	EXPECT_TRUE(movedTo.GetBufferAccesses().empty());
	EXPECT_FALSE(movedTo.HasRecordCallback());
}

TEST(Pass, test_pass_builder_is_fluent_authoring_surface)
{
	static_assert(!std::is_default_constructible_v<PassBuilder>);
	static_assert(!std::is_copy_constructible_v<PassBuilder>);
	static_assert(!std::is_copy_assignable_v<PassBuilder>);
	static_assert(!std::is_move_constructible_v<PassBuilder>);
	static_assert(!std::is_move_assignable_v<PassBuilder>);

	static_assert(std::same_as<decltype(std::declval<PassBuilder&>().ColorAttachment(
		std::declval<ImageHandle>(), std::declval<LoadOp>(), std::declval<ClearColor>())), PassBuilder&>);
	static_assert(std::same_as<decltype(std::declval<PassBuilder&>().DepthAttachment(
		std::declval<ImageHandle>(), std::declval<LoadOp>(), std::declval<float>())), PassBuilder&>);
	static_assert(std::same_as<decltype(std::declval<PassBuilder&>().Read(
		std::declval<ImageHandle>(), std::declval<ResourceUsage>())), PassBuilder&>);
	static_assert(std::same_as<decltype(std::declval<PassBuilder&>().Read(
		std::declval<BufferHandle>(), std::declval<ResourceUsage>())), PassBuilder&>);
	static_assert(std::same_as<decltype(std::declval<PassBuilder&>().Write(
		std::declval<ImageHandle>(), std::declval<ResourceUsage>())), PassBuilder&>);
	static_assert(std::same_as<decltype(std::declval<PassBuilder&>().Write(
		std::declval<BufferHandle>(), std::declval<ResourceUsage>())), PassBuilder&>);
	static_assert(std::same_as<decltype(std::declval<PassBuilder&>().Record(
		std::declval<std::function<void(const GraphContext&)>>())), PassBuilder&>);

	static_assert(!HasGetReads<Pass>);
	static_assert(!HasGetWrites<Pass>);
	static_assert(!HasImageReadGetter<Pass>);
	static_assert(!HasImageWriteGetter<Pass>);
	static_assert(!HasBufferReadGetter<Pass>);
	static_assert(!HasBufferWriteGetter<Pass>);

	SUCCEED();
}

TEST(Pass, test_pass_and_graph_context_thread_safety)
{
	static_assert(!std::is_copy_constructible_v<GraphContext>);
	static_assert(!std::is_move_constructible_v<GraphContext>);
	static_assert(!std::is_copy_constructible_v<PassBuilder>);
	static_assert(!std::is_move_constructible_v<PassBuilder>);
	static_assert(!std::is_copy_constructible_v<Pass>);
	static_assert(std::is_move_constructible_v<Pass>);

	SUCCEED();
}
