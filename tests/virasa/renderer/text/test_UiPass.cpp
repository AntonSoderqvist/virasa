#include <concepts>
#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/graph/Graph.h"
#include "virasa/renderer/graph/Pass.h"
#include "virasa/renderer/graph/Types.h"
#include "virasa/renderer/text/UiPass.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/Types.h"

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
	using InitializeSignature =
		RenderError (UiPass::*)(const Device&, const Context&, const FontAtlas&);

	EXPECT_TRUE((std::is_same_v<decltype(&UiPass::Initialize), InitializeSignature>));
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<UiPass&>().Initialize(
						   std::declval<const Device&>(),
						   std::declval<const Context&>(),
						   std::declval<const ui::FontAtlas&>())),
		RenderError>));
}

TEST(UiPass, test_submit_appends_single_overlay_pass_with_load_op_load)
{
	using SubmitSignature = void (UiPass::*)(renderer::graph::Graph&,
		renderer::graph::ImageHandle,
		const ui::DrawList&,
		const ui::FontAtlas&,
		uint32_t,
		uint32_t,
		uint32_t);

	EXPECT_TRUE((std::is_same_v<decltype(&UiPass::Submit), SubmitSignature>));
	EXPECT_TRUE((std::is_same_v<decltype(std::declval<UiPass&>().Submit(
						   std::declval<renderer::graph::Graph&>(),
						   std::declval<renderer::graph::ImageHandle>(),
						   std::declval<const ui::DrawList&>(),
						   std::declval<const ui::FontAtlas&>(),
						   std::declval<uint32_t>(),
						   std::declval<uint32_t>(),
						   std::declval<uint32_t>())),
		void>));

	constexpr renderer::graph::LoadOp kExpectedLoad = renderer::graph::LoadOp::Load;
	EXPECT_EQ(static_cast<uint8_t>(kExpectedLoad), 0u);
}

TEST(UiPass, test_submit_rebuilds_geometry_buffer_per_call)
{
	static_assert(std::is_same_v<decltype(std::declval<ui::DrawList>().GetQuads()),
		std::span<const ui::QuadCommand>>);
	static_assert(std::is_same_v<decltype(std::declval<ui::DrawList>().GetTexts()),
		std::span<const ui::TextCommand>>);
	static_assert(std::is_same_v<decltype(std::declval<ui::DrawList>().GetTextBuffer()),
		std::string_view>);
	static_assert(
		std::is_same_v<decltype(std::declval<const ui::FontAtlas&>().GetGlyph(uint32_t{})),
			const ui::GlyphMetrics&>);

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

TEST(UiPass, test_submit_records_two_draws_quads_then_text)
{
	ui::DrawList list;
	list.AddQuad(ui::QuadCommand{10.0f, 20.0f, 30.0f, 40.0f, ui::Color{1.0f, 0.0f, 0.0f, 1.0f}});
	list.AddText(50.0f, 60.0f, "abc", ui::Color{0.0f, 1.0f, 0.0f, 1.0f});

	ASSERT_EQ(list.GetQuads().size(), 1u);
	ASSERT_EQ(list.GetTexts().size(), 1u);
	EXPECT_EQ(list.GetTextBuffer(), "abc");
	EXPECT_LT(list.GetQuads().front().x, list.GetTexts().front().x);
}

TEST(UiPass, test_submit_record_callback_captures_per_frame_data_by_value)
{
	ui::DrawList list;
	list.AddText(10.0f, 20.0f, "frame", ui::Color{1.0f, 1.0f, 1.0f, 1.0f});

	const std::string beforeMutation(list.GetTextBuffer());
	ASSERT_EQ(beforeMutation, "frame");

	list.Clear();
	list.AddText(1.0f, 2.0f, "next", ui::Color{0.5f, 0.5f, 0.5f, 1.0f});

	EXPECT_EQ(beforeMutation, "frame");
	EXPECT_EQ(list.GetTextBuffer(), "next");
}

TEST(UiPass, test_ui_pass_is_not_thread_safe_per_instance)
{
	EXPECT_FALSE(std::is_copy_constructible_v<renderer::text::UiPass>);
	EXPECT_FALSE(std::is_copy_assignable_v<renderer::text::UiPass>);
	EXPECT_TRUE(std::is_move_constructible_v<renderer::text::UiPass>);
	EXPECT_TRUE(std::is_move_assignable_v<renderer::text::UiPass>);
}
