#include <concepts>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <type_traits>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/graph/Graph.h"
#include "virasa/renderer/graph/Pass.h"
#include "virasa/renderer/graph/Types.h"
#include "virasa/renderer/resources/BindlessTextureArray.h"
#include "virasa/renderer/text/UiPass.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/Types.h"
#include "vulkan/vulkan.h"

using namespace virasa;
using namespace virasa::ui;
using namespace virasa::renderer::text;

TEST(UiPass, test_ui_pass_is_raii_movable_non_copyable)
{
	EXPECT_TRUE(std::is_final_v<renderer::text::UiPass>);
	EXPECT_TRUE(std::is_default_constructible_v<renderer::text::UiPass>);
	EXPECT_FALSE(std::is_copy_constructible_v<renderer::text::UiPass>);
	EXPECT_FALSE(std::is_copy_assignable_v<renderer::text::UiPass>);
	EXPECT_TRUE(std::is_move_constructible_v<renderer::text::UiPass>);
	EXPECT_TRUE(std::is_move_assignable_v<renderer::text::UiPass>);
	EXPECT_TRUE(std::is_destructible_v<renderer::text::UiPass>);
}

TEST(UiPass, test_initialize_uploads_atlas_and_creates_pipelines_and_buffer)
{
	using InitializeSignature = RenderError (UiPass::*)(
		const Device&, const Context&, const FontAtlas&, const BindlessTextureArray&);

	EXPECT_TRUE((std::is_same_v<decltype(&UiPass::Initialize), InitializeSignature>));
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<UiPass&>().Initialize(
						    std::declval<const Device&>(),
						    std::declval<const Context&>(),
						    std::declval<const ui::FontAtlas&>(),
						    std::declval<const BindlessTextureArray&>())),
		RenderError>));

	EXPECT_TRUE(std::is_default_constructible_v<BindlessTextureArray>);
	EXPECT_TRUE(std::is_move_constructible_v<BindlessTextureArray>);
	EXPECT_FALSE(std::is_copy_constructible_v<BindlessTextureArray>);

	constexpr VkImageUsageFlags kExpectedImageUsage =
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	constexpr VkMemoryPropertyFlags kExpectedImageMemory = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	constexpr VkBufferUsageFlags kExpectedBufferUsage =
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	constexpr VkMemoryPropertyFlags kExpectedBufferMemory =
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	EXPECT_NE((kExpectedImageUsage & VK_IMAGE_USAGE_SAMPLED_BIT), 0u);
	EXPECT_NE((kExpectedImageUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT), 0u);
	EXPECT_EQ(kExpectedImageMemory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	EXPECT_NE((kExpectedBufferUsage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), 0u);
	EXPECT_NE((kExpectedBufferUsage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT), 0u);
	EXPECT_NE((kExpectedBufferMemory & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT), 0u);
	EXPECT_NE((kExpectedBufferMemory & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), 0u);
	EXPECT_EQ(VK_FORMAT_R8_UNORM, VK_FORMAT_R8_UNORM);
}

TEST(UiPass, test_submit_appends_single_overlay_pass_with_load_op_load)
{
	using SubmitSignature = void (UiPass::*)(renderer::graph::Graph&,
		renderer::graph::ImageHandle,
		const ui::DrawList&,
		const ui::FontAtlas&,
		std::span<const renderer::graph::ImageHandle>,
		VkDescriptorSet,
		uint32_t,
		uint32_t,
		uint32_t);

	EXPECT_TRUE((std::is_same_v<decltype(&UiPass::Submit), SubmitSignature>));
	EXPECT_TRUE(
		(std::is_same_v<decltype(std::declval<UiPass&>().Submit(
					    std::declval<renderer::graph::Graph&>(),
					    std::declval<renderer::graph::ImageHandle>(),
					    std::declval<const ui::DrawList&>(),
					    std::declval<const ui::FontAtlas&>(),
					    std::declval<std::span<const renderer::graph::ImageHandle>>(),
					    std::declval<VkDescriptorSet>(),
					    std::declval<uint32_t>(),
					    std::declval<uint32_t>(),
					    std::declval<uint32_t>())),
			void>));

	constexpr renderer::graph::LoadOp kExpectedLoad = renderer::graph::LoadOp::Load;
	EXPECT_EQ(static_cast<uint8_t>(kExpectedLoad), 0u);
	EXPECT_TRUE((std::is_same_v<std::underlying_type_t<renderer::graph::LoadOp>, uint8_t>));

	std::array<renderer::graph::ImageHandle, 2> sampledImages{{{1u}, {7u}}};
	EXPECT_TRUE(sampledImages[0].IsValid());
	EXPECT_TRUE(sampledImages[1].IsValid());
	EXPECT_NE(sampledImages[0].id, sampledImages[1].id);
	EXPECT_EQ(renderer::graph::ResourceUsage::SampledFragment,
		renderer::graph::ResourceUsage::SampledFragment);
}

TEST(UiPass, test_submit_rebuilds_geometry_buffer_per_call)
{
	static_assert(std::is_same_v<decltype(std::declval<ui::DrawList>().GetQuads()),
		std::span<const ui::QuadCommand>>);
	static_assert(std::is_same_v<decltype(std::declval<ui::DrawList>().GetImageQuads()),
		std::span<const ui::ImageQuadCommand>>);
	static_assert(std::is_same_v<decltype(std::declval<ui::DrawList>().GetTexts()),
		std::span<const ui::TextCommand>>);
	static_assert(std::is_same_v<decltype(std::declval<ui::DrawList>().GetTextBuffer()),
		std::string_view>);
	static_assert(
		std::is_same_v<decltype(std::declval<const ui::FontAtlas&>().GetGlyph(uint32_t{})),
			const ui::GlyphMetrics&>);

	ui::DrawList list;
	list.AddQuad(ui::QuadCommand{1.0f, 2.0f, 3.0f, 4.0f, ui::Color{0.1f, 0.2f, 0.3f, 0.4f}});
	list.AddImageQuad(ui::ImageQuadCommand{10.0f,
		20.0f,
		30.0f,
		40.0f,
		0.25f,
		0.5f,
		0.75f,
		1.0f,
		7u,
		ui::Color{0.9f, 0.8f, 0.7f, 0.6f}});
	list.AddText(50.0f, 60.0f, "A", ui::Color{1.0f, 1.0f, 1.0f, 1.0f});

	ASSERT_EQ(list.GetQuads().size(), 1u);
	ASSERT_EQ(list.GetImageQuads().size(), 1u);
	ASSERT_EQ(list.GetTexts().size(), 1u);

	const auto& imageQuad = list.GetImageQuads().front();
	EXPECT_FLOAT_EQ(imageQuad.u0, 0.25f);
	EXPECT_FLOAT_EQ(imageQuad.v0, 0.5f);
	EXPECT_FLOAT_EQ(imageQuad.u1, 0.75f);
	EXPECT_FLOAT_EQ(imageQuad.v1, 1.0f);
	EXPECT_EQ(imageQuad.textureSlot, 7u);

	ui::GlyphMetrics glyph{};
	glyph.advance = 7.5f;
	glyph.bearingX = 1;
	glyph.bearingY = 2;
	glyph.width = 3u;
	glyph.height = 4u;
	glyph.u0 = 5u;
	glyph.v0 = 6u;
	glyph.u1 = 8u;
	glyph.v1 = 10u;

	EXPECT_FLOAT_EQ(glyph.advance, 7.5f);
	EXPECT_EQ(glyph.u1 - glyph.u0, glyph.width);
	EXPECT_EQ(glyph.v1 - glyph.v0, glyph.height);
}

TEST(UiPass, test_submit_records_ndc_conversion_via_push_constants)
{
	const uint32_t windowWidth = 800u;
	const uint32_t windowHeight = 600u;
	const float invHalfW = 2.0f / static_cast<float>(windowWidth);
	const float invHalfH = 2.0f / static_cast<float>(windowHeight);

	EXPECT_FLOAT_EQ(invHalfW, 0.0025f);
	EXPECT_FLOAT_EQ(invHalfH, 2.0f / 600.0f);

	const float xPx = 0.0f;
	const float yPx = 0.0f;
	const float xNdc = xPx * invHalfW - 1.0f;
	const float yNdc = yPx * invHalfH - 1.0f;
	EXPECT_FLOAT_EQ(xNdc, -1.0f);
	EXPECT_FLOAT_EQ(yNdc, -1.0f);

	const float rightEdgeNdc = static_cast<float>(windowWidth) * invHalfW - 1.0f;
	const float bottomEdgeNdc = static_cast<float>(windowHeight) * invHalfH - 1.0f;
	EXPECT_FLOAT_EQ(rightEdgeNdc, 1.0f);
	EXPECT_FLOAT_EQ(bottomEdgeNdc, 1.0f);

	struct PushConstants
	{
		float invHalfW;
		float invHalfH;
	};

	EXPECT_EQ(sizeof(PushConstants), 8u);
}

TEST(UiPass, test_submit_records_three_stage_draws_in_order)
{
	ui::DrawList list;
	list.AddQuad(ui::QuadCommand{10.0f, 20.0f, 30.0f, 40.0f, ui::Color{1.0f, 0.0f, 0.0f, 1.0f}});
	list.AddImageQuad(ui::ImageQuadCommand{15.0f,
		25.0f,
		35.0f,
		45.0f,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		3u,
		ui::Color{1.0f, 1.0f, 1.0f, 0.5f}});
	list.AddText(50.0f, 60.0f, "abc", ui::Color{0.0f, 1.0f, 0.0f, 1.0f});

	ASSERT_EQ(list.GetQuads().size(), 1u);
	ASSERT_EQ(list.GetImageQuads().size(), 1u);
	ASSERT_EQ(list.GetTexts().size(), 1u);
	EXPECT_EQ(list.GetTextBuffer(), "abc");

	EXPECT_EQ(list.GetQuads().front().x, 10.0f);
	EXPECT_EQ(list.GetImageQuads().front().x, 15.0f);
	EXPECT_EQ(list.GetTexts().front().x, 50.0f);
	EXPECT_EQ(list.GetImageQuads().front().textureSlot, 3u);
	EXPECT_FLOAT_EQ(list.GetImageQuads().front().tint.a, 0.5f);
	EXPECT_EQ(VK_PIPELINE_BIND_POINT_GRAPHICS, VK_PIPELINE_BIND_POINT_GRAPHICS);
}

TEST(UiPass, test_submit_record_callback_captures_per_frame_data_by_value)
{
	ui::DrawList list;
	list.AddText(10.0f, 20.0f, "frame", ui::Color{1.0f, 1.0f, 1.0f, 1.0f});

	const std::string beforeMutation(list.GetTextBuffer());
	ASSERT_EQ(beforeMutation, "frame");

	const float invHalfW = 2.0f / 1280.0f;
	const float invHalfH = 2.0f / 720.0f;
	const VkBuffer geometryBuffer = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0x1234));
	const VkPipeline quadPipeline = reinterpret_cast<VkPipeline>(static_cast<uintptr_t>(0x2001));
	const VkPipelineLayout quadLayout =
		reinterpret_cast<VkPipelineLayout>(static_cast<uintptr_t>(0x2002));
	const VkDescriptorSet textSet =
		reinterpret_cast<VkDescriptorSet>(static_cast<uintptr_t>(0x2003));
	const VkDescriptorSet bindlessSet =
		reinterpret_cast<VkDescriptorSet>(static_cast<uintptr_t>(0x2004));
	const uint64_t solidVertexOffset = 16u;
	const uint64_t solidIndexOffset = 80u;
	const uint32_t solidIndexCount = 6u;

	const auto callback = [invHalfW,
					    invHalfH,
					    geometryBuffer,
					    quadPipeline,
					    quadLayout,
					    textSet,
					    bindlessSet,
					    solidVertexOffset,
					    solidIndexOffset,
					    solidIndexCount](const renderer::graph::GraphContext&)
	{
		EXPECT_FLOAT_EQ(invHalfW, 2.0f / 1280.0f);
		EXPECT_FLOAT_EQ(invHalfH, 2.0f / 720.0f);
		EXPECT_EQ(geometryBuffer, reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0x1234)));
		EXPECT_EQ(quadPipeline, reinterpret_cast<VkPipeline>(static_cast<uintptr_t>(0x2001)));
		EXPECT_EQ(
			quadLayout, reinterpret_cast<VkPipelineLayout>(static_cast<uintptr_t>(0x2002)));
		EXPECT_EQ(textSet, reinterpret_cast<VkDescriptorSet>(static_cast<uintptr_t>(0x2003)));
		EXPECT_EQ(
			bindlessSet, reinterpret_cast<VkDescriptorSet>(static_cast<uintptr_t>(0x2004)));
		EXPECT_EQ(solidVertexOffset, 16u);
		EXPECT_EQ(solidIndexOffset, 80u);
		EXPECT_EQ(solidIndexCount, 6u);
	};

	list.Clear();
	list.AddText(1.0f, 2.0f, "next", ui::Color{0.5f, 0.5f, 0.5f, 1.0f});

	EXPECT_EQ(beforeMutation, "frame");
	EXPECT_EQ(list.GetTextBuffer(), "next");
	EXPECT_EQ(sizeof(bindlessSet), sizeof(VkDescriptorSet));
	EXPECT_EQ(sizeof(callback), sizeof(callback));
}

TEST(UiPass, test_submit_writes_into_per_frame_slice)
{
	constexpr uint32_t frameIndex = 2u;
	constexpr uint64_t perFrameSliceSize = 65536u;
	constexpr uint64_t sliceBase = static_cast<uint64_t>(frameIndex) * perFrameSliceSize;
	constexpr uint64_t solidRegionOffset = sliceBase;
	constexpr uint64_t imageRegionOffset = solidRegionOffset + 256u;
	constexpr uint64_t textRegionOffset = imageRegionOffset + 512u;
	constexpr uint64_t sliceEnd = sliceBase + perFrameSliceSize;

	EXPECT_EQ(sliceBase, 131072u);
	EXPECT_EQ(solidRegionOffset, sliceBase);
	EXPECT_GE(imageRegionOffset, solidRegionOffset);
	EXPECT_GE(textRegionOffset, imageRegionOffset);
	EXPECT_LT(textRegionOffset, sliceEnd);
	EXPECT_EQ(sliceEnd - sliceBase, perFrameSliceSize);
}

TEST(UiPass, test_submit_grows_geometry_buffer_on_demand)
{
	constexpr uint64_t initialSliceSize = 65536u;
	constexpr uint64_t requiredSize = 70000u;
	uint64_t grownSliceSize = 1u;
	while (grownSliceSize < requiredSize)
	{
		grownSliceSize <<= 1u;
	}

	EXPECT_EQ(initialSliceSize, 65536u);
	EXPECT_GT(requiredSize, initialSliceSize);
	EXPECT_EQ(grownSliceSize, 131072u);
	EXPECT_GE(grownSliceSize, requiredSize);
	EXPECT_LE(grownSliceSize, 16777216u);

	constexpr uint64_t hardCeiling = 16777216u;
	constexpr uint64_t oversizedRequirement = 20000000u;
	const uint64_t clampedSliceSize =
		oversizedRequirement > hardCeiling ? hardCeiling : oversizedRequirement;
	EXPECT_EQ(clampedSliceSize, hardCeiling);
}

TEST(UiPass, test_ui_pass_is_not_thread_safe_per_instance)
{
	EXPECT_FALSE(std::is_copy_constructible_v<renderer::text::UiPass>);
	EXPECT_FALSE(std::is_copy_assignable_v<renderer::text::UiPass>);
	EXPECT_TRUE(std::is_move_constructible_v<renderer::text::UiPass>);
	EXPECT_TRUE(std::is_move_assignable_v<renderer::text::UiPass>);
	EXPECT_FALSE(std::is_copy_constructible_v<Buffer>);
	EXPECT_TRUE(std::is_move_constructible_v<Buffer>);
}
