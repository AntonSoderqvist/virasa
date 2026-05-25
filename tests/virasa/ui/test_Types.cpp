#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "virasa/ui/Types.h"

using namespace virasa::ui;

TEST(Types, test_color_is_linear_rgba_float)
{
	static_assert(std::is_default_constructible_v<Color>);
	static_assert(std::is_copy_constructible_v<Color>);
	static_assert(std::is_copy_assignable_v<Color>);
	static_assert(std::is_move_constructible_v<Color>);
	static_assert(std::is_move_assignable_v<Color>);
	static_assert(std::is_trivially_destructible_v<Color>);
	static_assert(std::is_standard_layout_v<Color>);

	static_assert(std::is_same_v<decltype(Color::r), float>);
	static_assert(std::is_same_v<decltype(Color::g), float>);
	static_assert(std::is_same_v<decltype(Color::b), float>);
	static_assert(std::is_same_v<decltype(Color::a), float>);

	static_assert(offsetof(Color, r) == 0);
	static_assert(offsetof(Color, g) == sizeof(float));
	static_assert(offsetof(Color, b) == sizeof(float) * 2);
	static_assert(offsetof(Color, a) == sizeof(float) * 3);

	Color color;
	EXPECT_FLOAT_EQ(color.r, 0.0f);
	EXPECT_FLOAT_EQ(color.g, 0.0f);
	EXPECT_FLOAT_EQ(color.b, 0.0f);
	EXPECT_FLOAT_EQ(color.a, 1.0f);

	color.r = -0.5f;
	color.g = 1.5f;
	color.b = 2.0f;
	color.a = -1.0f;

	EXPECT_FLOAT_EQ(color.r, -0.5f);
	EXPECT_FLOAT_EQ(color.g, 1.5f);
	EXPECT_FLOAT_EQ(color.b, 2.0f);
	EXPECT_FLOAT_EQ(color.a, -1.0f);
}

TEST(Types, test_draw_command_kind_enum_values_in_declared_order)
{
	static_assert(std::is_enum_v<DrawCommandKind>);
	static_assert(std::is_same_v<std::underlying_type_t<DrawCommandKind>, uint32_t>);

	EXPECT_EQ(static_cast<uint32_t>(DrawCommandKind::Quad), 0u);
	EXPECT_EQ(static_cast<uint32_t>(DrawCommandKind::Text), 1u);
	EXPECT_EQ(static_cast<uint32_t>(DrawCommandKind::ImageQuad), 2u);
	EXPECT_LT(static_cast<uint32_t>(DrawCommandKind::Quad),
		static_cast<uint32_t>(DrawCommandKind::Text));
	EXPECT_LT(static_cast<uint32_t>(DrawCommandKind::Text),
		static_cast<uint32_t>(DrawCommandKind::ImageQuad));
}

TEST(Types, test_quad_command_describes_solid_rectangle)
{
	static_assert(std::is_default_constructible_v<QuadCommand>);
	static_assert(std::is_copy_constructible_v<QuadCommand>);
	static_assert(std::is_copy_assignable_v<QuadCommand>);
	static_assert(std::is_move_constructible_v<QuadCommand>);
	static_assert(std::is_move_assignable_v<QuadCommand>);
	static_assert(std::is_trivially_destructible_v<QuadCommand>);
	static_assert(std::is_standard_layout_v<QuadCommand>);

	static_assert(std::is_same_v<decltype(QuadCommand::x), float>);
	static_assert(std::is_same_v<decltype(QuadCommand::y), float>);
	static_assert(std::is_same_v<decltype(QuadCommand::width), float>);
	static_assert(std::is_same_v<decltype(QuadCommand::height), float>);
	static_assert(std::is_same_v<decltype(QuadCommand::color), Color>);

	static_assert(offsetof(QuadCommand, x) == 0);
	static_assert(offsetof(QuadCommand, y) == sizeof(float));
	static_assert(offsetof(QuadCommand, width) == sizeof(float) * 2);
	static_assert(offsetof(QuadCommand, height) == sizeof(float) * 3);
	static_assert(offsetof(QuadCommand, color) >= sizeof(float) * 4);

	QuadCommand quad;
	EXPECT_FLOAT_EQ(quad.x, 0.0f);
	EXPECT_FLOAT_EQ(quad.y, 0.0f);
	EXPECT_FLOAT_EQ(quad.width, 0.0f);
	EXPECT_FLOAT_EQ(quad.height, 0.0f);
	EXPECT_FLOAT_EQ(quad.color.r, 0.0f);
	EXPECT_FLOAT_EQ(quad.color.g, 0.0f);
	EXPECT_FLOAT_EQ(quad.color.b, 0.0f);
	EXPECT_FLOAT_EQ(quad.color.a, 1.0f);

	quad.x = 10.5f;
	quad.y = 20.25f;
	quad.width = -3.0f;
	quad.height = 0.0f;
	quad.color = Color{0.25f, 0.5f, 0.75f, 0.125f};

	EXPECT_FLOAT_EQ(quad.x, 10.5f);
	EXPECT_FLOAT_EQ(quad.y, 20.25f);
	EXPECT_FLOAT_EQ(quad.width, -3.0f);
	EXPECT_FLOAT_EQ(quad.height, 0.0f);
	EXPECT_FLOAT_EQ(quad.color.r, 0.25f);
	EXPECT_FLOAT_EQ(quad.color.g, 0.5f);
	EXPECT_FLOAT_EQ(quad.color.b, 0.75f);
	EXPECT_FLOAT_EQ(quad.color.a, 0.125f);
}

TEST(Types, test_text_command_references_draw_list_text_buffer)
{
	static_assert(std::is_default_constructible_v<TextCommand>);
	static_assert(std::is_copy_constructible_v<TextCommand>);
	static_assert(std::is_copy_assignable_v<TextCommand>);
	static_assert(std::is_move_constructible_v<TextCommand>);
	static_assert(std::is_move_assignable_v<TextCommand>);
	static_assert(std::is_trivially_destructible_v<TextCommand>);
	static_assert(std::is_standard_layout_v<TextCommand>);

	static_assert(std::is_same_v<decltype(TextCommand::x), float>);
	static_assert(std::is_same_v<decltype(TextCommand::y), float>);
	static_assert(std::is_same_v<decltype(TextCommand::textOffset), uint32_t>);
	static_assert(std::is_same_v<decltype(TextCommand::textLength), uint32_t>);
	static_assert(std::is_same_v<decltype(TextCommand::color), Color>);

	static_assert(offsetof(TextCommand, x) == 0);
	static_assert(offsetof(TextCommand, y) == sizeof(float));
	static_assert(offsetof(TextCommand, textOffset) == sizeof(float) * 2);
	static_assert(offsetof(TextCommand, textLength) == sizeof(float) * 2 + sizeof(uint32_t));
	static_assert(offsetof(TextCommand, color) >= sizeof(float) * 2 + sizeof(uint32_t) * 2);

	TextCommand text;
	EXPECT_FLOAT_EQ(text.x, 0.0f);
	EXPECT_FLOAT_EQ(text.y, 0.0f);
	EXPECT_EQ(text.textOffset, 0u);
	EXPECT_EQ(text.textLength, 0u);
	EXPECT_FLOAT_EQ(text.color.r, 0.0f);
	EXPECT_FLOAT_EQ(text.color.g, 0.0f);
	EXPECT_FLOAT_EQ(text.color.b, 0.0f);
	EXPECT_FLOAT_EQ(text.color.a, 1.0f);

	DrawList drawList;
	const Color firstColor{0.1f, 0.2f, 0.3f, 0.4f};
	const Color secondColor{0.6f, 0.7f, 0.8f, 0.9f};
	const std::string firstTextBytes = "h\xc3\xa9";
	const std::string secondTextBytes = "!";

	drawList.AddText(12.5f, 34.75f, firstTextBytes, firstColor);
	drawList.AddText(-1.0f, 2.5f, secondTextBytes, secondColor);

	const auto texts = drawList.GetTexts();
	const std::string_view buffer = drawList.GetTextBuffer();
	ASSERT_EQ(texts.size(), 2u);

	EXPECT_FLOAT_EQ(texts[0].x, 12.5f);
	EXPECT_FLOAT_EQ(texts[0].y, 34.75f);
	EXPECT_EQ(texts[0].textOffset, 0u);
	EXPECT_EQ(texts[0].textLength, static_cast<uint32_t>(firstTextBytes.size()));
	EXPECT_FLOAT_EQ(texts[0].color.r, firstColor.r);
	EXPECT_FLOAT_EQ(texts[0].color.g, firstColor.g);
	EXPECT_FLOAT_EQ(texts[0].color.b, firstColor.b);
	EXPECT_FLOAT_EQ(texts[0].color.a, firstColor.a);

	EXPECT_FLOAT_EQ(texts[1].x, -1.0f);
	EXPECT_FLOAT_EQ(texts[1].y, 2.5f);
	EXPECT_EQ(texts[1].textOffset, static_cast<uint32_t>(firstTextBytes.size()));
	EXPECT_EQ(texts[1].textLength, static_cast<uint32_t>(secondTextBytes.size()));
	EXPECT_FLOAT_EQ(texts[1].color.r, secondColor.r);
	EXPECT_FLOAT_EQ(texts[1].color.g, secondColor.g);
	EXPECT_FLOAT_EQ(texts[1].color.b, secondColor.b);
	EXPECT_FLOAT_EQ(texts[1].color.a, secondColor.a);

	ASSERT_LE(static_cast<size_t>(texts[0].textOffset + texts[0].textLength), buffer.size());
	ASSERT_LE(static_cast<size_t>(texts[1].textOffset + texts[1].textLength), buffer.size());
	EXPECT_EQ(buffer.substr(texts[0].textOffset, texts[0].textLength), firstTextBytes);
	EXPECT_EQ(buffer.substr(texts[1].textOffset, texts[1].textLength), secondTextBytes);
}

TEST(Types, test_draw_list_owns_text_buffer)
{
	static_assert(std::is_default_constructible_v<DrawList>);
	static_assert(std::is_copy_constructible_v<DrawList>);
	static_assert(std::is_copy_assignable_v<DrawList>);
	static_assert(std::is_move_constructible_v<DrawList>);
	static_assert(std::is_move_assignable_v<DrawList>);
	static_assert(std::is_final_v<DrawList>);
	static_assert(std::is_same_v<decltype(std::declval<const DrawList&>().GetQuads()),
		std::span<const QuadCommand>>);
	static_assert(std::is_same_v<decltype(std::declval<const DrawList&>().GetTexts()),
		std::span<const TextCommand>>);
	static_assert(std::is_same_v<decltype(std::declval<const DrawList&>().GetImageQuads()),
		std::span<const ImageQuadCommand>>);
	static_assert(std::is_same_v<decltype(std::declval<const DrawList&>().GetTextBuffer()),
		std::string_view>);
	static_assert(noexcept(std::declval<const DrawList&>().GetQuads()));
	static_assert(noexcept(std::declval<const DrawList&>().GetTexts()));
	static_assert(noexcept(std::declval<const DrawList&>().GetImageQuads()));
	static_assert(noexcept(std::declval<const DrawList&>().GetTextBuffer()));

	DrawList drawList;
	EXPECT_TRUE(drawList.GetQuads().empty());
	EXPECT_TRUE(drawList.GetTexts().empty());
	EXPECT_TRUE(drawList.GetImageQuads().empty());
	EXPECT_TRUE(drawList.GetTextBuffer().empty());

	const QuadCommand firstQuad{1.0f, 2.0f, 3.0f, 4.0f, Color{0.1f, 0.2f, 0.3f, 0.4f}};
	const QuadCommand secondQuad{5.0f, 6.0f, 7.0f, 8.0f, Color{0.5f, 0.6f, 0.7f, 0.8f}};
	const ImageQuadCommand firstImageQuad{
		11.0f, 12.0f, 13.0f, 14.0f, 0.1f, 0.2f, 0.8f, 0.9f, 7u, Color{0.9f, 0.8f, 0.7f, 0.6f}};
	const ImageQuadCommand secondImageQuad{
		21.0f, 22.0f, 23.0f, 24.0f, -1.0f, 2.0f, 3.0f, 4.0f, 9u, Color{0.2f, 0.3f, 0.4f, 0.5f}};
	const Color firstTextColor{0.9f, 0.8f, 0.7f, 0.6f};
	const Color secondTextColor{0.2f, 0.3f, 0.4f, 0.5f};

	drawList.AddQuad(firstQuad);
	drawList.AddQuad(secondQuad);
	drawList.AddText(10.0f, 20.0f, "abc", firstTextColor);
	drawList.AddText(30.0f, 40.0f, "de", secondTextColor);
	drawList.AddImageQuad(firstImageQuad);
	drawList.AddImageQuad(secondImageQuad);

	const auto quads = drawList.GetQuads();
	ASSERT_EQ(quads.size(), 2u);
	EXPECT_FLOAT_EQ(quads[0].x, firstQuad.x);
	EXPECT_FLOAT_EQ(quads[0].y, firstQuad.y);
	EXPECT_FLOAT_EQ(quads[0].width, firstQuad.width);
	EXPECT_FLOAT_EQ(quads[0].height, firstQuad.height);
	EXPECT_FLOAT_EQ(quads[1].x, secondQuad.x);
	EXPECT_FLOAT_EQ(quads[1].y, secondQuad.y);
	EXPECT_FLOAT_EQ(quads[1].width, secondQuad.width);
	EXPECT_FLOAT_EQ(quads[1].height, secondQuad.height);

	const auto texts = drawList.GetTexts();
	ASSERT_EQ(texts.size(), 2u);
	EXPECT_FLOAT_EQ(texts[0].x, 10.0f);
	EXPECT_FLOAT_EQ(texts[0].y, 20.0f);
	EXPECT_EQ(texts[0].textOffset, 0u);
	EXPECT_EQ(texts[0].textLength, 3u);
	EXPECT_FLOAT_EQ(texts[1].x, 30.0f);
	EXPECT_FLOAT_EQ(texts[1].y, 40.0f);
	EXPECT_EQ(texts[1].textOffset, 3u);
	EXPECT_EQ(texts[1].textLength, 2u);
	EXPECT_EQ(drawList.GetTextBuffer(), "abcde");

	const auto imageQuads = drawList.GetImageQuads();
	ASSERT_EQ(imageQuads.size(), 2u);
	EXPECT_FLOAT_EQ(imageQuads[0].x, firstImageQuad.x);
	EXPECT_FLOAT_EQ(imageQuads[0].y, firstImageQuad.y);
	EXPECT_FLOAT_EQ(imageQuads[0].width, firstImageQuad.width);
	EXPECT_FLOAT_EQ(imageQuads[0].height, firstImageQuad.height);
	EXPECT_FLOAT_EQ(imageQuads[0].u0, firstImageQuad.u0);
	EXPECT_FLOAT_EQ(imageQuads[0].v0, firstImageQuad.v0);
	EXPECT_FLOAT_EQ(imageQuads[0].u1, firstImageQuad.u1);
	EXPECT_FLOAT_EQ(imageQuads[0].v1, firstImageQuad.v1);
	EXPECT_EQ(imageQuads[0].textureSlot, firstImageQuad.textureSlot);
	EXPECT_FLOAT_EQ(imageQuads[1].x, secondImageQuad.x);
	EXPECT_FLOAT_EQ(imageQuads[1].y, secondImageQuad.y);
	EXPECT_FLOAT_EQ(imageQuads[1].width, secondImageQuad.width);
	EXPECT_FLOAT_EQ(imageQuads[1].height, secondImageQuad.height);
	EXPECT_FLOAT_EQ(imageQuads[1].u0, secondImageQuad.u0);
	EXPECT_FLOAT_EQ(imageQuads[1].v0, secondImageQuad.v0);
	EXPECT_FLOAT_EQ(imageQuads[1].u1, secondImageQuad.u1);
	EXPECT_FLOAT_EQ(imageQuads[1].v1, secondImageQuad.v1);
	EXPECT_EQ(imageQuads[1].textureSlot, secondImageQuad.textureSlot);

	DrawList copied = drawList;
	ASSERT_EQ(copied.GetQuads().size(), 2u);
	ASSERT_EQ(copied.GetTexts().size(), 2u);
	ASSERT_EQ(copied.GetImageQuads().size(), 2u);
	EXPECT_EQ(copied.GetTextBuffer(), "abcde");

	drawList.Clear();
	EXPECT_TRUE(drawList.GetQuads().empty());
	EXPECT_TRUE(drawList.GetTexts().empty());
	EXPECT_TRUE(drawList.GetImageQuads().empty());
	EXPECT_TRUE(drawList.GetTextBuffer().empty());

	ASSERT_EQ(copied.GetTexts().size(), 2u);
	ASSERT_EQ(copied.GetImageQuads().size(), 2u);
	EXPECT_EQ(copied.GetTextBuffer(), "abcde");

	DrawList moved = std::move(copied);
	ASSERT_EQ(moved.GetQuads().size(), 2u);
	ASSERT_EQ(moved.GetTexts().size(), 2u);
	ASSERT_EQ(moved.GetImageQuads().size(), 2u);
	EXPECT_EQ(moved.GetTextBuffer(), "abcde");
}

TEST(Types, test_draw_list_pixel_coordinate_convention)
{
	DrawList drawList;

	const QuadCommand quad{-10.5f, 20.25f, 30.75f, 40.5f, Color{1.0f, 0.0f, 0.0f, 1.0f}};
	const ImageQuadCommand imageQuad{-300.25f,
		400.5f,
		64.75f,
		32.125f,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		3u,
		Color{0.25f, 0.5f, 0.75f, 1.0f}};
	drawList.AddQuad(quad);
	drawList.AddImageQuad(imageQuad);
	drawList.AddText(100.125f, -200.5f, "pixel-space", Color{0.0f, 1.0f, 0.0f, 0.5f});

	const auto quads = drawList.GetQuads();
	const auto imageQuads = drawList.GetImageQuads();
	const auto texts = drawList.GetTexts();
	ASSERT_EQ(quads.size(), 1u);
	ASSERT_EQ(imageQuads.size(), 1u);
	ASSERT_EQ(texts.size(), 1u);

	EXPECT_FLOAT_EQ(quads[0].x, -10.5f);
	EXPECT_FLOAT_EQ(quads[0].y, 20.25f);
	EXPECT_FLOAT_EQ(quads[0].width, 30.75f);
	EXPECT_FLOAT_EQ(quads[0].height, 40.5f);

	EXPECT_FLOAT_EQ(imageQuads[0].x, -300.25f);
	EXPECT_FLOAT_EQ(imageQuads[0].y, 400.5f);
	EXPECT_FLOAT_EQ(imageQuads[0].width, 64.75f);
	EXPECT_FLOAT_EQ(imageQuads[0].height, 32.125f);

	EXPECT_FLOAT_EQ(texts[0].x, 100.125f);
	EXPECT_FLOAT_EQ(texts[0].y, -200.5f);
	EXPECT_EQ(drawList.GetTextBuffer(), "pixel-space");
}
