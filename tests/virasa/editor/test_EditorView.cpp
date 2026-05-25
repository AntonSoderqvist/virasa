#include <gtest/gtest.h>

#include "virasa/editor/EditorView.h"
#include "virasa/editor/Buffer.h"
#include "virasa/editor/Motions.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/TextPanel.h"
#include "virasa/ui/Types.h"
#include "virasa/window/Events.h"

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace virasa;
using namespace virasa::editor;
using namespace virasa::ui;

namespace
{

constexpr bool FloatEq(const float a, const float b) noexcept
{
	return a == b;
}

} // namespace

TEST(EditorView, test_editor_view_owns_buffer_motion_state_and_panel)
{
	static_assert(std::is_final_v<EditorView>);
	static_assert(std::is_default_constructible_v<EditorView>);
	static_assert(std::is_copy_constructible_v<EditorView>);
	static_assert(std::is_copy_assignable_v<EditorView>);
	static_assert(std::is_move_constructible_v<EditorView>);
	static_assert(std::is_move_assignable_v<EditorView>);

	EditorView view;

	EXPECT_TRUE(view.GetBuffer().GetText().empty());
	EXPECT_EQ(view.GetBuffer().GetCursorByte(), 0u);
	EXPECT_EQ(view.GetMotionState().GetMode(), Mode::Normal);

	view.GetBuffer().SetText("hello");
	view.GetBuffer().SetCursorByte(2u);
	view.GetMotionState().SetMode(Mode::Insert);

	TextPanelConfig config = view.GetPanel().GetConfig();
	config.paddingX = 11.0f;
	config.paddingY = 7.0f;
	view.GetPanel().SetConfig(config);

	EditorView copied = view;
	EXPECT_EQ(copied.GetBuffer().GetText(), std::string_view("hello"));
	EXPECT_EQ(copied.GetBuffer().GetCursorByte(), 2u);
	EXPECT_EQ(copied.GetMotionState().GetMode(), Mode::Insert);
	EXPECT_TRUE(FloatEq(copied.GetPanel().GetConfig().paddingX, 11.0f));
	EXPECT_TRUE(FloatEq(copied.GetPanel().GetConfig().paddingY, 7.0f));

	EditorView moved = std::move(copied);
	EXPECT_EQ(moved.GetBuffer().GetText(), std::string_view("hello"));
	EXPECT_EQ(moved.GetBuffer().GetCursorByte(), 2u);
	EXPECT_EQ(moved.GetMotionState().GetMode(), Mode::Insert);
	EXPECT_TRUE(FloatEq(moved.GetPanel().GetConfig().paddingX, 11.0f));
	EXPECT_TRUE(FloatEq(moved.GetPanel().GetConfig().paddingY, 7.0f));
}

TEST(EditorView, test_editor_view_key_result_enum_values_in_declared_order)
{
	static_assert(std::is_same_v<std::underlying_type_t<EditorViewKeyResult>, uint8_t>);

	EXPECT_EQ(static_cast<uint8_t>(EditorViewKeyResult::Consumed), 0u);
	EXPECT_EQ(static_cast<uint8_t>(EditorViewKeyResult::RequestCommandBar), 1u);
}

TEST(EditorView, test_get_buffer_returns_owned_buffer)
{
	EditorView view;

	Buffer& buffer = view.GetBuffer();
	buffer.SetText("abc");
	buffer.SetCursorByte(1u);

	EXPECT_EQ(&buffer, &view.GetBuffer());
	EXPECT_EQ(view.GetBuffer().GetText(), std::string_view("abc"));
	EXPECT_EQ(view.GetBuffer().GetCursorByte(), 1u);

	const EditorView& constView = view;
	const Buffer& constBuffer = constView.GetBuffer();
	EXPECT_EQ(&constBuffer, &view.GetBuffer());
	EXPECT_EQ(constBuffer.GetText(), std::string_view("abc"));
	EXPECT_EQ(constBuffer.GetCursorByte(), 1u);
}

TEST(EditorView, test_get_motion_state_returns_owned_motion_state)
{
	EditorView view;

	MotionState& motionState = view.GetMotionState();
	motionState.SetMode(Mode::Insert);

	EXPECT_EQ(&motionState, &view.GetMotionState());
	EXPECT_EQ(view.GetMotionState().GetMode(), Mode::Insert);

	const EditorView& constView = view;
	const MotionState& constMotionState = constView.GetMotionState();
	EXPECT_EQ(&constMotionState, &view.GetMotionState());
	EXPECT_EQ(constMotionState.GetMode(), Mode::Insert);
}

TEST(EditorView, test_get_panel_returns_owned_text_panel)
{
	EditorView view;

	TextPanel& panel = view.GetPanel();
	TextPanelConfig config = panel.GetConfig();
	config.paddingX = 13.0f;
	config.lineSpacing = 1.5f;
	panel.SetConfig(config);

	EXPECT_EQ(&panel, &view.GetPanel());
	EXPECT_TRUE(FloatEq(view.GetPanel().GetConfig().paddingX, 13.0f));
	EXPECT_TRUE(FloatEq(view.GetPanel().GetConfig().lineSpacing, 1.5f));

	const EditorView& constView = view;
	const TextPanel& constPanel = constView.GetPanel();
	EXPECT_EQ(&constPanel, &view.GetPanel());
	EXPECT_TRUE(FloatEq(constPanel.GetConfig().paddingX, 13.0f));
	EXPECT_TRUE(FloatEq(constPanel.GetConfig().lineSpacing, 1.5f));
}

TEST(EditorView, test_handle_key_forwards_to_motion_state)
{
	EditorView view;
	TextPanelConfig originalConfig = view.GetPanel().GetConfig();

	view.GetBuffer().SetText("abc");
	view.GetBuffer().SetCursorByte(1u);
	view.GetMotionState().SetMode(Mode::Insert);

	EXPECT_EQ(view.HandleKey(KeyCode::Backspace), EditorViewKeyResult::Consumed);
	EXPECT_EQ(view.GetBuffer().GetText(), std::string_view("bc"));
	EXPECT_EQ(view.GetBuffer().GetCursorByte(), 0u);
	EXPECT_EQ(view.GetMotionState().GetMode(), Mode::Insert);
	EXPECT_TRUE(FloatEq(view.GetPanel().GetConfig().paddingX, originalConfig.paddingX));
	EXPECT_TRUE(FloatEq(view.GetPanel().GetConfig().paddingY, originalConfig.paddingY));
	EXPECT_TRUE(FloatEq(view.GetPanel().GetConfig().lineSpacing, originalConfig.lineSpacing));

	view.GetMotionState().SetMode(Mode::Normal);
	view.GetBuffer().SetText("bc");
	view.GetBuffer().SetCursorByte(0u);

	EXPECT_EQ(view.HandleKey(KeyCode::Escape), EditorViewKeyResult::Consumed);
	EXPECT_EQ(view.GetBuffer().GetText(), std::string_view("bc"));
	EXPECT_EQ(view.GetBuffer().GetCursorByte(), 0u);
	EXPECT_EQ(view.GetMotionState().GetMode(), Mode::Normal);
}

TEST(EditorView, test_handle_text_input_forwards_to_motion_state)
{
	EditorView view;
	TextPanelConfig originalConfig = view.GetPanel().GetConfig();

	view.GetMotionState().SetMode(Mode::Insert);
	view.GetBuffer().SetText("ac");
	view.GetBuffer().SetCursorByte(1u);

	EXPECT_EQ(view.HandleTextInput("b"), EditorViewKeyResult::Consumed);
	EXPECT_EQ(view.GetBuffer().GetText(), std::string_view("abc"));
	EXPECT_EQ(view.GetBuffer().GetCursorByte(), 2u);
	EXPECT_EQ(view.GetMotionState().GetMode(), Mode::Insert);
	EXPECT_TRUE(FloatEq(view.GetPanel().GetConfig().paddingX, originalConfig.paddingX));
	EXPECT_TRUE(FloatEq(view.GetPanel().GetConfig().paddingY, originalConfig.paddingY));
	EXPECT_TRUE(FloatEq(view.GetPanel().GetConfig().lineSpacing, originalConfig.lineSpacing));

	view.GetMotionState().SetMode(Mode::Normal);
	view.GetBuffer().SetText("abc");
	view.GetBuffer().SetCursorByte(1u);

	EXPECT_EQ(view.HandleTextInput(":"), EditorViewKeyResult::RequestCommandBar);
	EXPECT_EQ(view.GetBuffer().GetText(), std::string_view("abc"));
	EXPECT_EQ(view.GetBuffer().GetCursorByte(), 1u);
	EXPECT_EQ(view.GetMotionState().GetMode(), Mode::Normal);
}

TEST(EditorView, test_render_delegates_to_owned_text_panel)
{
	EditorView view;
	view.GetBuffer().SetText("first\n\nsecond");

	TextPanelConfig config = view.GetPanel().GetConfig();
	config.background = {0.2f, 0.3f, 0.4f, 0.5f};
	config.foreground = {0.8f, 0.7f, 0.6f, 1.0f};
	config.paddingX = 9.0f;
	config.paddingY = 3.0f;
	config.lineSpacing = 1.25f;
	view.GetPanel().SetConfig(config);

	const std::string_view textBefore = view.GetBuffer().GetText();
	const std::size_t cursorBefore = view.GetBuffer().GetCursorByte();
	const Mode modeBefore = view.GetMotionState().GetMode();

	DrawList out;
	QuadCommand existingQuad;
	existingQuad.x = 1.0f;
	existingQuad.y = 2.0f;
	existingQuad.width = 3.0f;
	existingQuad.height = 4.0f;
	out.AddQuad(existingQuad);
	out.AddText(5.0f, 6.0f, "prefix", {1.0f, 1.0f, 1.0f, 1.0f});

	FontAtlas atlas;
	view.Render(out, atlas, 10.0f, 20.0f, 300.0f, 120.0f);

	EXPECT_EQ(view.GetBuffer().GetText(), textBefore);
	EXPECT_EQ(view.GetBuffer().GetCursorByte(), cursorBefore);
	EXPECT_EQ(view.GetMotionState().GetMode(), modeBefore);

	ASSERT_EQ(out.GetQuads().size(), 2u);
	EXPECT_TRUE(FloatEq(out.GetQuads()[1].x, 10.0f));
	EXPECT_TRUE(FloatEq(out.GetQuads()[1].y, 20.0f));
	EXPECT_TRUE(FloatEq(out.GetQuads()[1].width, 300.0f));
	EXPECT_TRUE(FloatEq(out.GetQuads()[1].height, 120.0f));
	EXPECT_TRUE(FloatEq(out.GetQuads()[1].color.r, 0.2f));
	EXPECT_TRUE(FloatEq(out.GetQuads()[1].color.g, 0.3f));
	EXPECT_TRUE(FloatEq(out.GetQuads()[1].color.b, 0.4f));
	EXPECT_TRUE(FloatEq(out.GetQuads()[1].color.a, 0.5f));

	ASSERT_EQ(out.GetTexts().size(), 3u);
	EXPECT_EQ(out.GetImageQuads().size(), 0u);
	EXPECT_EQ(out.GetTextBuffer().substr(0u, 6u), std::string_view("prefix"));

	const TextCommand& firstLine = out.GetTexts()[1];
	const TextCommand& secondLine = out.GetTexts()[2];
	const std::string_view textBuffer = out.GetTextBuffer();
	const float ascender = atlas.GetAscender();
	const float descender = atlas.GetDescender();
	const float lineHeight = ascender - descender;
	const float advance = lineHeight * config.lineSpacing;

	EXPECT_TRUE(FloatEq(firstLine.x, 19.0f));
	EXPECT_TRUE(FloatEq(firstLine.y, 23.0f + ascender));
	EXPECT_EQ(textBuffer.substr(firstLine.textOffset, firstLine.textLength), std::string_view("first"));
	EXPECT_TRUE(FloatEq(firstLine.color.r, 0.8f));
	EXPECT_TRUE(FloatEq(firstLine.color.g, 0.7f));
	EXPECT_TRUE(FloatEq(firstLine.color.b, 0.6f));
	EXPECT_TRUE(FloatEq(firstLine.color.a, 1.0f));

	EXPECT_TRUE(FloatEq(secondLine.x, 19.0f));
	EXPECT_TRUE(FloatEq(secondLine.y, 23.0f + ascender + 2.0f * advance));
	EXPECT_EQ(textBuffer.substr(secondLine.textOffset, secondLine.textLength), std::string_view("second"));
	EXPECT_TRUE(FloatEq(secondLine.color.r, 0.8f));
	EXPECT_TRUE(FloatEq(secondLine.color.g, 0.7f));
	EXPECT_TRUE(FloatEq(secondLine.color.b, 0.6f));
	EXPECT_TRUE(FloatEq(secondLine.color.a, 1.0f));
}
