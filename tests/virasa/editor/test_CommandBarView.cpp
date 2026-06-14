#include <gtest/gtest.h>

#include "virasa/editor/CommandBarView.h"
#include "virasa/ui/CommandBar.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/Types.h"
#include "virasa/window/Events.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace virasa;

TEST(CommandBarView, test_command_bar_view_owns_panel_text_and_cursor)
{
	using virasa::editor::CommandBarView;

	static_assert(std::is_final_v<CommandBarView>);
	static_assert(std::is_default_constructible_v<CommandBarView>);
	static_assert(std::is_copy_constructible_v<CommandBarView>);
	static_assert(std::is_copy_assignable_v<CommandBarView>);
	static_assert(std::is_move_constructible_v<CommandBarView>);
	static_assert(std::is_move_assignable_v<CommandBarView>);

	CommandBarView view;
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);

	view.SetText("abc");
	EXPECT_EQ(view.GetText(), std::string_view("abc"));
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 3u);

	view.SetText("");
	EXPECT_EQ(view.GetCursorByte(), 1u);
	ASSERT_LT(view.GetCursorByte(), view.GetText().size() + 1u);
	if (view.GetCursorByte() != 0u && view.GetCursorByte() != view.GetText().size())
	{
		EXPECT_NE((static_cast<unsigned char>(view.GetText()[view.GetCursorByte()]) & 0xC0u), 0x80u);
	}

	view.SetText("\x80");
	EXPECT_EQ(view.GetCursorByte(), 2u);

	view.SetText("\x80\x81");
	EXPECT_EQ(view.GetCursorByte(), 3u);

	CommandBarView copied = view;
	EXPECT_EQ(copied.GetText(), std::string_view("\x80\x81", 3));
	EXPECT_TRUE(copied.GetSubmittedArgument().empty());
	EXPECT_EQ(copied.GetCursorByte(), 3u);

	view.SetText(":load model.glb");
	EXPECT_EQ(view.Submit(), virasa::editor::CommandResult::LoadModel);
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("model.glb"));

	copied = view;
	EXPECT_TRUE(copied.GetText().empty());
	EXPECT_EQ(copied.GetSubmittedArgument(), std::string_view("model.glb"));
	EXPECT_EQ(copied.GetCursorByte(), 0u);

	view.Clear();
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);
	EXPECT_EQ(copied.GetSubmittedArgument(), std::string_view("model.glb"));

	CommandBarView moved = std::move(copied);
	EXPECT_TRUE(moved.GetText().empty());
	EXPECT_EQ(moved.GetSubmittedArgument(), std::string_view("model.glb"));
	EXPECT_EQ(moved.GetCursorByte(), 0u);
}

TEST(CommandBarView, test_command_bar_key_result_enum_values_in_declared_order)
{
	using virasa::editor::CommandBarKeyResult;

	static_assert(std::is_same_v<std::underlying_type_t<CommandBarKeyResult>, uint8_t>);
	EXPECT_EQ(static_cast<uint8_t>(CommandBarKeyResult::Consumed), 0u);
	EXPECT_EQ(static_cast<uint8_t>(CommandBarKeyResult::Submitted), 1u);
}

TEST(CommandBarView, test_command_result_enum_values_in_declared_order)
{
	using virasa::editor::CommandResult;

	static_assert(std::is_same_v<std::underlying_type_t<CommandResult>, uint8_t>);
	EXPECT_EQ(static_cast<uint8_t>(CommandResult::None), 0u);
	EXPECT_EQ(static_cast<uint8_t>(CommandResult::OpenEditor), 1u);
	EXPECT_EQ(static_cast<uint8_t>(CommandResult::OpenHierarchy), 2u);
	EXPECT_EQ(static_cast<uint8_t>(CommandResult::Close), 3u);
	EXPECT_EQ(static_cast<uint8_t>(CommandResult::Quit), 4u);
	EXPECT_EQ(static_cast<uint8_t>(CommandResult::LoadModel), 5u);
	EXPECT_EQ(static_cast<uint8_t>(CommandResult::Play), 6u);
	EXPECT_EQ(static_cast<uint8_t>(CommandResult::Stop), 7u);
}

TEST(CommandBarView, test_get_text_returns_internal_byte_view)
{
	virasa::editor::CommandBarView view;
	view.SetText("seed");

	const std::string_view text = view.GetText();
	EXPECT_EQ(text, std::string_view("seed"));
	EXPECT_EQ(text.data(), view.GetText().data());
	EXPECT_NO_THROW(static_cast<void>(view.GetText()));
}

TEST(CommandBarView, test_get_cursor_byte_returns_current_offset)
{
	virasa::editor::CommandBarView view;
	EXPECT_EQ(view.GetCursorByte(), 0u);
	EXPECT_LE(view.GetCursorByte(), view.GetText().size());

	view.SetText("hello");
	EXPECT_EQ(view.GetCursorByte(), 5u);
	EXPECT_LE(view.GetCursorByte(), view.GetText().size());

	view.HandleTextInput("!");
	EXPECT_EQ(view.GetCursorByte(), 6u);
	EXPECT_LE(view.GetCursorByte(), view.GetText().size());
}

TEST(CommandBarView, test_get_submitted_argument_returns_last_argument)
{
	virasa::editor::CommandBarView view;

	const std::string_view initial = view.GetSubmittedArgument();
	EXPECT_TRUE(initial.empty());
	EXPECT_EQ(initial.data(), view.GetSubmittedArgument().data());
	EXPECT_NO_THROW(static_cast<void>(view.GetSubmittedArgument()));

	view.SetText(":load assets/model.glb");
	EXPECT_EQ(view.Submit(), virasa::editor::CommandResult::LoadModel);

	const std::string_view argument = view.GetSubmittedArgument();
	EXPECT_EQ(argument, std::string_view("assets/model.glb"));
	EXPECT_EQ(argument.data(), view.GetSubmittedArgument().data());

	view.SetText(":q");
	EXPECT_EQ(view.Submit(), virasa::editor::CommandResult::Quit);
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
}

TEST(CommandBarView, test_clear_resets_text_and_cursor)
{
	virasa::editor::CommandBarView view;
	view.SetText(":load command.glb");
	EXPECT_EQ(view.Submit(), virasa::editor::CommandResult::LoadModel);
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("command.glb"));
	view.SetText("command");

	virasa::ui::CommandBarConfig config;
	config.paddingX = 17.0f;
	view.GetPanel().SetConfig(config);

	view.Clear();

	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);
	EXPECT_FLOAT_EQ(view.GetPanel().GetConfig().paddingX, 17.0f);
}

TEST(CommandBarView, test_set_text_replaces_text_and_places_cursor_at_end)
{
	virasa::editor::CommandBarView view;

	view.SetText(":load old.glb");
	EXPECT_EQ(view.Submit(), virasa::editor::CommandResult::LoadModel);
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("old.glb"));

	view.SetText("old");
	view.SetText(std::string_view("a\0b", 3));

	EXPECT_EQ(view.GetText().size(), 3u);
	EXPECT_EQ(view.GetText(), std::string_view("a\0b", 3));
	EXPECT_EQ(view.GetCursorByte(), 3u);
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("old.glb"));
}

TEST(CommandBarView, test_get_panel_returns_underlying_command_bar)
{
	virasa::editor::CommandBarView view;
	view.SetText("text");

	virasa::ui::CommandBar& panel = view.GetPanel();
	const virasa::editor::CommandBarView& constView = view;
	const virasa::ui::CommandBar& constPanel = constView.GetPanel();

	EXPECT_EQ(&panel, &constPanel);
	EXPECT_EQ(view.GetText(), std::string_view("text"));
	EXPECT_EQ(view.GetCursorByte(), 4u);

	virasa::ui::CommandBarConfig config;
	config.paddingY = 9.0f;
	panel.SetConfig(config);
	EXPECT_FLOAT_EQ(constPanel.GetConfig().paddingY, 9.0f);
	EXPECT_EQ(view.GetText(), std::string_view("text"));
	EXPECT_EQ(view.GetCursorByte(), 4u);
}

TEST(CommandBarView, test_handle_key_dispatches_non_printable_keys)
{
	using virasa::editor::CommandBarKeyResult;
	using virasa::editor::CommandBarView;

	CommandBarView view;
	view.SetText(":load keep.glb");
	EXPECT_EQ(view.Submit(), virasa::editor::CommandResult::LoadModel);
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("keep.glb"));

	EXPECT_EQ(view.HandleKey(KeyCode::A), CommandBarKeyResult::Consumed);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("keep.glb"));

	view.SetText("A\xC3\xA9" "B");
	EXPECT_EQ(view.GetCursorByte(), 4u);

	EXPECT_EQ(view.HandleKey(KeyCode::Backspace), CommandBarKeyResult::Consumed);
	EXPECT_EQ(view.GetText(), std::string_view("A\xC3\xA9"));
	EXPECT_EQ(view.GetCursorByte(), 3u);
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("keep.glb"));

	EXPECT_EQ(view.HandleKey(KeyCode::Backspace), CommandBarKeyResult::Consumed);
	EXPECT_EQ(view.GetText(), std::string_view("A"));
	EXPECT_EQ(view.GetCursorByte(), 1u);
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("keep.glb"));

	EXPECT_EQ(view.HandleKey(KeyCode::Backspace), CommandBarKeyResult::Consumed);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("keep.glb"));

	EXPECT_EQ(view.HandleKey(KeyCode::Backspace), CommandBarKeyResult::Consumed);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("keep.glb"));

	view.SetText(":q");
	EXPECT_EQ(view.HandleKey(KeyCode::Enter), CommandBarKeyResult::Submitted);
	EXPECT_EQ(view.GetText(), std::string_view(":q"));
	EXPECT_EQ(view.GetCursorByte(), 2u);
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("keep.glb"));

	EXPECT_EQ(view.HandleKey(KeyCode::Escape), CommandBarKeyResult::Consumed);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);
}

TEST(CommandBarView, test_handle_text_input_inserts_bytes_at_cursor)
{
	using virasa::editor::CommandBarKeyResult;

	virasa::editor::CommandBarView view;
	view.SetText(":load arg.glb");
	EXPECT_EQ(view.Submit(), virasa::editor::CommandResult::LoadModel);
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("arg.glb"));

	EXPECT_EQ(view.HandleTextInput(""), CommandBarKeyResult::Consumed);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("arg.glb"));

	view.SetText("ab");
	EXPECT_EQ(view.HandleTextInput(std::string_view("\0x", 2)), CommandBarKeyResult::Consumed);
	EXPECT_EQ(view.GetText(), std::string_view("ab\0x", 4));
	EXPECT_EQ(view.GetCursorByte(), 4u);
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("arg.glb"));
}

TEST(CommandBarView, test_submit_parses_text_and_returns_command_result)
{
	using virasa::editor::CommandResult;
	using virasa::editor::CommandBarView;

	CommandBarView view;

	EXPECT_EQ(view.Submit(), CommandResult::Close);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);

	view.SetText(":");
	EXPECT_EQ(view.Submit(), CommandResult::Close);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);

	view.SetText(":ide");
	EXPECT_EQ(view.Submit(), CommandResult::OpenEditor);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);

	view.SetText(":tree");
	EXPECT_EQ(view.Submit(), CommandResult::OpenHierarchy);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);

	view.SetText(":q");
	EXPECT_EQ(view.Submit(), CommandResult::Quit);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);

	view.SetText(":load scene.glb");
	EXPECT_EQ(view.Submit(), CommandResult::LoadModel);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_EQ(view.GetSubmittedArgument(), std::string_view("scene.glb"));
	EXPECT_EQ(view.GetCursorByte(), 0u);

	view.SetText(":play");
	EXPECT_EQ(view.Submit(), CommandResult::Play);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);

	view.SetText(":stop");
	EXPECT_EQ(view.Submit(), CommandResult::Stop);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);

	view.SetText(":load ");
	EXPECT_EQ(view.Submit(), CommandResult::None);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);

	view.SetText(":play now");
	EXPECT_EQ(view.Submit(), CommandResult::None);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);

	virasa::ui::CommandBarConfig config;
	config.paddingX = 23.0f;
	view.GetPanel().SetConfig(config);
	view.SetText(":IDE");
	EXPECT_EQ(view.Submit(), CommandResult::None);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);
	EXPECT_FLOAT_EQ(view.GetPanel().GetConfig().paddingX, 23.0f);

	view.SetText(":Stop");
	EXPECT_EQ(view.Submit(), CommandResult::None);
	EXPECT_TRUE(view.GetText().empty());
	EXPECT_TRUE(view.GetSubmittedArgument().empty());
	EXPECT_EQ(view.GetCursorByte(), 0u);
}

TEST(CommandBarView, test_render_delegates_to_owned_command_bar)
{
	virasa::editor::CommandBarView view;
	view.SetText("abc");

	virasa::ui::CommandBarConfig config;
	config.background = {0.2f, 0.3f, 0.4f, 0.5f};
	config.foreground = {0.6f, 0.7f, 0.8f, 0.9f};
	config.cursorColor = {0.1f, 0.2f, 0.3f, 0.4f};
	config.paddingX = 11.0f;
	config.paddingY = 5.0f;
	view.GetPanel().SetConfig(config);

	virasa::ui::DrawList drawList;
	virasa::ui::QuadCommand existingQuad;
	existingQuad.x = 123.0f;
	drawList.AddQuad(existingQuad);

	virasa::ui::FontAtlas atlas;
	const auto quadsBefore = drawList.GetQuads().size();
	const auto textsBefore = drawList.GetTexts().size();
	const auto imagesBefore = drawList.GetImageQuads().size();

	view.Render(drawList, atlas, 320u, 200u);

	EXPECT_EQ(drawList.GetQuads().size(), quadsBefore + 2u);
	EXPECT_EQ(drawList.GetTexts().size(), textsBefore + 1u);
	EXPECT_EQ(drawList.GetImageQuads().size(), imagesBefore);
	EXPECT_EQ(view.GetText(), std::string_view("abc"));
	EXPECT_EQ(view.GetCursorByte(), 3u);

	const auto quads = drawList.GetQuads();
	const auto texts = drawList.GetTexts();
	ASSERT_GE(quads.size(), 3u);
	ASSERT_GE(texts.size(), 1u);

	const float lineHeight = atlas.GetAscender() - atlas.GetDescender();
	const float barHeight = lineHeight + 2.0f * config.paddingY;
	const float barTop = 200.0f - barHeight;
	const float expectedCursorY = barTop + (barHeight - lineHeight) * 0.5f;
	const float expectedCursorWidth =
		atlas.GetGlyph(0x20).advance == 0.0f ? 0.6f * lineHeight : atlas.GetGlyph(0x20).advance;

	const auto& background = quads[1];
	EXPECT_FLOAT_EQ(background.x, 0.0f);
	EXPECT_FLOAT_EQ(background.y, barTop);
	EXPECT_FLOAT_EQ(background.width, 320.0f);
	EXPECT_FLOAT_EQ(background.height, barHeight);
	EXPECT_FLOAT_EQ(background.color.r, config.background.r);
	EXPECT_FLOAT_EQ(background.color.g, config.background.g);
	EXPECT_FLOAT_EQ(background.color.b, config.background.b);
	EXPECT_FLOAT_EQ(background.color.a, config.background.a);

	const auto& text = texts[0];
	EXPECT_FLOAT_EQ(text.x, config.paddingX);
	EXPECT_FLOAT_EQ(text.y, barTop + config.paddingY + atlas.GetAscender());
	EXPECT_EQ(drawList.GetTextBuffer().substr(text.textOffset, text.textLength), std::string_view("abc"));
	EXPECT_FLOAT_EQ(text.color.r, config.foreground.r);
	EXPECT_FLOAT_EQ(text.color.g, config.foreground.g);
	EXPECT_FLOAT_EQ(text.color.b, config.foreground.b);
	EXPECT_FLOAT_EQ(text.color.a, config.foreground.a);

	const auto& cursor = quads[2];
	EXPECT_FLOAT_EQ(cursor.x, config.paddingX);
	EXPECT_FLOAT_EQ(cursor.y, expectedCursorY);
	EXPECT_FLOAT_EQ(cursor.width, expectedCursorWidth);
	EXPECT_FLOAT_EQ(cursor.height, lineHeight);
	EXPECT_FLOAT_EQ(cursor.color.r, config.cursorColor.r);
	EXPECT_FLOAT_EQ(cursor.color.g, config.cursorColor.g);
	EXPECT_FLOAT_EQ(cursor.color.b, config.cursorColor.b);
	EXPECT_FLOAT_EQ(cursor.color.a, config.cursorColor.a);
}
