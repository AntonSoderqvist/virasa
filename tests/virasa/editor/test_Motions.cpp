#include <gtest/gtest.h>

#include "virasa/editor/Motions.h"
#include "virasa/editor/Buffer.h"
#include "virasa/window/Events.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace virasa;
using namespace virasa::editor;

namespace
{

bool IsWhitespaceByte(unsigned char byte)
{
	return byte == 0x20 || byte == 0x09 || byte == 0x0A || byte == 0x0D;
}

std::size_t ComputeNextWordStart(const Buffer& buffer)
{
	const std::string_view text = buffer.GetText();
	std::size_t index = buffer.GetCursorByte();

	while (index < text.size() && !IsWhitespaceByte(static_cast<unsigned char>(text[index])))
	{
		++index;
	}

	while (index < text.size() && IsWhitespaceByte(static_cast<unsigned char>(text[index])))
	{
		++index;
	}

	return index;
}

std::size_t ComputePreviousWordStart(const Buffer& buffer)
{
	const std::string_view text = buffer.GetText();
	const std::size_t cursor = buffer.GetCursorByte();
	if (cursor == 0)
	{
		return 0;
	}

	std::size_t index = cursor - 1;
	while (true)
	{
		if (!IsWhitespaceByte(static_cast<unsigned char>(text[index])))
		{
			break;
		}
		if (index == 0)
		{
			return 0;
		}
		--index;
	}

	while (index > 0 && !IsWhitespaceByte(static_cast<unsigned char>(text[index - 1])))
	{
		--index;
	}

	return index;
}

} // namespace

TEST(MotionState, test_mode_enum_values_in_declared_order)
{
	static_assert(std::is_enum_v<Mode>);
	static_assert(std::is_same_v<std::underlying_type_t<Mode>, std::uint8_t>);
	static_assert(static_cast<std::uint8_t>(Mode::Normal) == 0u);
	static_assert(static_cast<std::uint8_t>(Mode::Insert) == 1u);

	MotionState motion;
	EXPECT_EQ(motion.GetMode(), Mode::Normal);
}

TEST(MotionState, test_key_result_enum_values_in_declared_order)
{
	static_assert(std::is_enum_v<KeyResult>);
	static_assert(std::is_same_v<std::underlying_type_t<KeyResult>, std::uint8_t>);
	static_assert(static_cast<std::uint8_t>(KeyResult::Consumed) == 0u);
	static_assert(static_cast<std::uint8_t>(KeyResult::RequestCommandBar) == 1u);

	EXPECT_NE(KeyResult::Consumed, KeyResult::RequestCommandBar);
}

TEST(MotionState, test_pending_operator_enum_values_in_declared_order)
{
	static_assert(std::is_enum_v<PendingOperator>);
	static_assert(std::is_same_v<std::underlying_type_t<PendingOperator>, std::uint8_t>);
	static_assert(static_cast<std::uint8_t>(PendingOperator::None) == 0u);
	static_assert(static_cast<std::uint8_t>(PendingOperator::Delete) == 1u);

	EXPECT_NE(PendingOperator::None, PendingOperator::Delete);
}

TEST(MotionState, test_motion_state_default_constructs_to_normal_mode_with_no_pending_state)
{
	static_assert(std::is_final_v<MotionState>);
	static_assert(std::is_default_constructible_v<MotionState>);
	static_assert(std::is_copy_constructible_v<MotionState>);
	static_assert(std::is_copy_assignable_v<MotionState>);
	static_assert(std::is_move_constructible_v<MotionState>);
	static_assert(std::is_move_assignable_v<MotionState>);

	MotionState original;
	EXPECT_EQ(original.GetMode(), Mode::Normal);

	MotionState copied(original);
	EXPECT_EQ(copied.GetMode(), Mode::Normal);

	MotionState moved(std::move(copied));
	EXPECT_EQ(moved.GetMode(), Mode::Normal);
}

TEST(MotionState, test_get_mode_returns_current_mode)
{
	const MotionState initial;
	EXPECT_EQ(initial.GetMode(), Mode::Normal);

	MotionState motion;
	motion.SetMode(Mode::Insert);
	EXPECT_EQ(motion.GetMode(), Mode::Insert);

	motion.SetMode(Mode::Normal);
	EXPECT_EQ(motion.GetMode(), Mode::Normal);
}

TEST(MotionState, test_set_mode_replaces_mode_and_clears_pending_state)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("alpha\nbeta\n");
	buffer.SetCursorByte(buffer.GetLineStartByte(1));

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);

	buffer.SetCursorByte(buffer.GetLineStartByte(1));
	motion.SetMode(Mode::Insert);
	EXPECT_EQ(motion.GetMode(), Mode::Insert);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "alpha\ngbeta\n");
	EXPECT_EQ(buffer.GetCursorByte(), 7u);

	motion.SetMode(Mode::Insert);
	EXPECT_EQ(motion.GetMode(), Mode::Insert);
	EXPECT_EQ(motion.HandleTextInput("x", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "alpha\ngxbeta\n");
	EXPECT_EQ(buffer.GetCursorByte(), 8u);

	motion.SetMode(Mode::Normal);
	EXPECT_EQ(motion.GetMode(), Mode::Normal);
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 8u);
	motion.SetMode(Mode::Normal);
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 8u);
}

TEST(MotionState, test_reset_clears_pending_state_without_changing_mode)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("alpha\nbeta");
	buffer.SetCursorByte(buffer.GetLineStartByte(1));

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	motion.Reset();
	EXPECT_EQ(motion.GetMode(), Mode::Normal);
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), buffer.GetLineStartByte(1));
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);

	motion.SetMode(Mode::Insert);
	buffer.SetCursorByte(buffer.GetText().size());
	motion.Reset();
	EXPECT_EQ(motion.GetMode(), Mode::Insert);
	EXPECT_EQ(motion.HandleTextInput("!", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "alpha\nbeta!");
}

TEST(MotionState, test_handle_key_dispatches_by_mode_for_non_printable_keys)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("ab");
	buffer.SetCursorByte(1);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleKey(KeyCode::Left, buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "ab");
	EXPECT_EQ(buffer.GetCursorByte(), 1u);
	EXPECT_EQ(motion.GetMode(), Mode::Normal);
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);

	buffer.SetCursorByte(1);
	EXPECT_EQ(motion.HandleTextInput("d", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleKey(KeyCode::Escape, buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "ab");
	EXPECT_EQ(buffer.GetCursorByte(), 1u);
	EXPECT_EQ(motion.HandleTextInput("d", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "ab");

	motion.SetMode(Mode::Insert);
	buffer.SetCursorByte(1);
	EXPECT_EQ(motion.HandleKey(KeyCode::Left, buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "ab");
	EXPECT_EQ(buffer.GetCursorByte(), 1u);
	EXPECT_EQ(motion.GetMode(), Mode::Insert);

	EXPECT_EQ(motion.HandleKey(KeyCode::Enter, buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "a\nb");
	EXPECT_EQ(buffer.GetCursorByte(), 2u);

	EXPECT_EQ(motion.HandleKey(KeyCode::Backspace, buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "ab");
	EXPECT_EQ(buffer.GetCursorByte(), 1u);

	EXPECT_EQ(motion.HandleKey(KeyCode::Escape, buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.GetMode(), Mode::Normal);
}

TEST(MotionState, test_handle_text_input_dispatches_by_mode_for_printable_input)
{
	MotionState motion;
	Buffer normalBuffer;
	normalBuffer.SetText("abc");
	normalBuffer.SetCursorByte(1);

	EXPECT_EQ(motion.HandleTextInput("l", normalBuffer), KeyResult::Consumed);
	EXPECT_EQ(normalBuffer.GetCursorByte(), 2u);
	EXPECT_EQ(normalBuffer.GetText(), "abc");

	EXPECT_EQ(motion.HandleTextInput(":ignored", normalBuffer), KeyResult::RequestCommandBar);
	EXPECT_EQ(normalBuffer.GetText(), "abc");
	EXPECT_EQ(normalBuffer.GetCursorByte(), 2u);
	EXPECT_EQ(motion.GetMode(), Mode::Normal);

	motion.SetMode(Mode::Insert);
	Buffer insertBuffer;
	insertBuffer.SetText("abc");
	insertBuffer.SetCursorByte(1);
	EXPECT_EQ(motion.HandleTextInput("XYZ", insertBuffer), KeyResult::Consumed);
	EXPECT_EQ(insertBuffer.GetText(), "aXYZbc");
	EXPECT_EQ(insertBuffer.GetCursorByte(), 4u);
}

TEST(MotionState, test_word_class_partitions_text_into_whitespace_and_word_runs)
{
	MotionState motion;
	Buffer buffer;
	const std::string text = std::string("ab\t") + "\xC3\xA9" + " cd\r\nef";
	buffer.SetText(text);

	buffer.SetCursorByte(0);
	EXPECT_EQ(motion.HandleTextInput("w", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 3u);

	EXPECT_EQ(motion.HandleTextInput("w", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 6u);

	EXPECT_EQ(motion.HandleTextInput("w", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 10u);

	EXPECT_EQ(motion.HandleTextInput("b", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 6u);

	EXPECT_EQ(motion.HandleTextInput("b", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 3u);

	EXPECT_EQ(motion.HandleTextInput("b", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);
}

TEST(MotionState, test_normal_h_moves_cursor_left_one_codepoint_within_line)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("a\xC3\xA9\nb");
	buffer.SetCursorByte(3);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("h", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 1u);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);

	EXPECT_EQ(motion.HandleTextInput("h", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);
}

TEST(MotionState, test_normal_l_moves_cursor_right_one_codepoint_within_line)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("a\xC3\xA9\nb");
	buffer.SetCursorByte(0);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("l", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 1u);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);

	buffer.SetCursorByte(buffer.GetLineEndByte(0));
	EXPECT_EQ(motion.HandleTextInput("l", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), buffer.GetLineEndByte(0));
}

TEST(MotionState, test_normal_j_moves_cursor_to_same_column_byte_on_next_line)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("abcd\n\xC3\xA9z\nq");
	buffer.SetCursorByte(3);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("j", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorLine(), 1u);
	EXPECT_EQ(buffer.GetCursorByte(), 8u);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);

	buffer.SetCursorByte(buffer.GetLineStartByte(buffer.GetLineCount() - 1));
	EXPECT_EQ(motion.HandleTextInput("j", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorLine(), buffer.GetLineCount() - 1);
}

TEST(MotionState, test_normal_k_moves_cursor_to_same_column_byte_on_previous_line)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("q\n\xC3\xA9z\nabcd");
	buffer.SetCursorByte(buffer.GetLineStartByte(2) + 3);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("k", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorLine(), 1u);
	EXPECT_EQ(buffer.GetCursorByte(), 5u);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);

	buffer.SetCursorByte(0);
	EXPECT_EQ(motion.HandleTextInput("k", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);
}

TEST(MotionState, test_normal_0_moves_cursor_to_line_start)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("abc\ndef");
	buffer.SetCursorByte(buffer.GetLineStartByte(1) + 2);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("0", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), buffer.GetLineStartByte(1));

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);
}

TEST(MotionState, test_normal_dollar_moves_cursor_to_line_end)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("a\xC3\xA9\n\nxyz");
	buffer.SetCursorByte(0);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("$", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 1u);

	buffer.SetCursorByte(buffer.GetLineStartByte(1));
	EXPECT_EQ(motion.HandleTextInput("$", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), buffer.GetLineStartByte(1));
}

TEST(MotionState, test_normal_gg_moves_cursor_to_first_line_start)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("one\ntwo\nthree");
	buffer.SetCursorByte(buffer.GetLineStartByte(2));

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), buffer.GetLineStartByte(2));
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);

	buffer.SetCursorByte(buffer.GetLineStartByte(1));
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("h", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), buffer.GetLineStartByte(1));
}

TEST(MotionState, test_normal_G_moves_cursor_to_last_line_start)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("one\ntwo\nthree");
	buffer.SetCursorByte(0);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("G", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), buffer.GetLineStartByte(buffer.GetLineCount() - 1));
}

TEST(MotionState, test_normal_i_enters_insert_mode_at_cursor)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("abc");
	buffer.SetCursorByte(1);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("i", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.GetMode(), Mode::Insert);
	EXPECT_EQ(buffer.GetCursorByte(), 1u);
	EXPECT_EQ(buffer.GetText(), "abc");

	EXPECT_EQ(motion.HandleTextInput("Z", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "aZbc");
	EXPECT_EQ(buffer.GetCursorByte(), 2u);
}

TEST(MotionState, test_normal_a_enters_insert_mode_after_cursor)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("a\xC3\xA9\nq");
	buffer.SetCursorByte(0);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("a", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.GetMode(), Mode::Insert);
	EXPECT_EQ(buffer.GetCursorByte(), 1u);
	EXPECT_EQ(buffer.GetText(), "a\xC3\xA9\nq");

	motion.SetMode(Mode::Normal);
	buffer.SetCursorByte(buffer.GetLineEndByte(0));
	EXPECT_EQ(motion.HandleTextInput("a", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.GetMode(), Mode::Insert);
	EXPECT_EQ(buffer.GetCursorByte(), buffer.GetLineEndByte(0));
}

TEST(MotionState, test_normal_o_opens_line_below_and_enters_insert)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("one\ntwo");
	buffer.SetCursorByte(1);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("o", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.GetMode(), Mode::Insert);
	EXPECT_EQ(buffer.GetText(), "one\n\ntwo");
	EXPECT_EQ(buffer.GetCursorByte(), buffer.GetLineStartByte(1));
}

TEST(MotionState, test_normal_O_opens_line_above_and_enters_insert)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("one\ntwo");
	buffer.SetCursorByte(buffer.GetLineStartByte(1) + 1);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("O", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.GetMode(), Mode::Insert);
	EXPECT_EQ(buffer.GetText(), "one\n\ntwo");
	EXPECT_EQ(buffer.GetCursorByte(), buffer.GetLineStartByte(1));
}

TEST(MotionState, test_normal_x_deletes_codepoint_under_cursor)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("a\xC3\xA9" "b\n");
	buffer.SetCursorByte(1);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("x", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "ab\n");
	EXPECT_EQ(buffer.GetCursorByte(), 1u);

	buffer.SetCursorByte(buffer.GetLineEndByte(0));
	EXPECT_EQ(motion.HandleTextInput("x", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "ab\n");
}

TEST(MotionState, test_normal_dd_deletes_current_line)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("one\ntwo\nthree");
	buffer.SetCursorByte(buffer.GetLineStartByte(1) + 1);

	EXPECT_EQ(motion.HandleTextInput("d", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "one\ntwo\nthree");
	EXPECT_EQ(motion.HandleTextInput("d", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "one\nthree");
	EXPECT_EQ(buffer.GetCursorByte(), buffer.GetLineStartByte(1));

	buffer.SetCursorByte(buffer.GetLineStartByte(buffer.GetLineCount() - 1));
	EXPECT_EQ(motion.HandleTextInput("d", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("d", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "one\n");

	buffer.SetText("one\ntwo");
	buffer.SetCursorByte(buffer.GetLineStartByte(1));
	EXPECT_EQ(motion.HandleTextInput("d", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput("h", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "one\ntwo");
	EXPECT_EQ(buffer.GetCursorByte(), buffer.GetLineStartByte(1));
}

TEST(MotionState, test_normal_w_moves_cursor_to_next_word_start)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("ab  cd\tef\r\n gh");
	buffer.SetCursorByte(0);

	const std::size_t expected1 = ComputeNextWordStart(buffer);
	EXPECT_EQ(motion.HandleTextInput("w", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), expected1);

	const std::size_t expected2 = ComputeNextWordStart(buffer);
	EXPECT_EQ(motion.HandleTextInput("w", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), expected2);

	const std::size_t expected3 = ComputeNextWordStart(buffer);
	EXPECT_EQ(motion.HandleTextInput("w", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), expected3);

	const std::size_t expected4 = ComputeNextWordStart(buffer);
	EXPECT_EQ(motion.HandleTextInput("w", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), expected4);
	EXPECT_EQ(buffer.GetCursorByte(), buffer.GetText().size());
}

TEST(MotionState, test_normal_b_moves_cursor_to_previous_word_start)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("ab  cd\tef\r\n gh");
	buffer.SetCursorByte(buffer.GetText().size());

	const std::size_t expected1 = ComputePreviousWordStart(buffer);
	EXPECT_EQ(motion.HandleTextInput("b", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), expected1);

	const std::size_t expected2 = ComputePreviousWordStart(buffer);
	EXPECT_EQ(motion.HandleTextInput("b", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), expected2);

	const std::size_t expected3 = ComputePreviousWordStart(buffer);
	EXPECT_EQ(motion.HandleTextInput("b", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), expected3);

	buffer.SetCursorByte(0);
	EXPECT_EQ(motion.HandleTextInput("b", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);
}

TEST(MotionState, test_normal_colon_requests_command_bar)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("abc");
	buffer.SetCursorByte(1);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleTextInput(":xyz", buffer), KeyResult::RequestCommandBar);
	EXPECT_EQ(buffer.GetText(), "abc");
	EXPECT_EQ(buffer.GetCursorByte(), 1u);
	EXPECT_EQ(motion.GetMode(), Mode::Normal);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 1u);
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);
}

TEST(MotionState, test_normal_escape_clears_pending_state)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("abc");
	buffer.SetCursorByte(1);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.HandleKey(KeyCode::Escape, buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.GetMode(), Mode::Normal);
	EXPECT_EQ(buffer.GetText(), "abc");
	EXPECT_EQ(buffer.GetCursorByte(), 1u);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 1u);
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);
}

TEST(MotionState, test_insert_text_input_inserts_at_cursor)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("ac");
	buffer.SetCursorByte(1);
	motion.SetMode(Mode::Insert);

	EXPECT_EQ(motion.HandleTextInput("b\xC3\xA9", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "ab\xC3\xA9" "c");
	EXPECT_EQ(buffer.GetCursorByte(), 4u);
	EXPECT_EQ(motion.GetMode(), Mode::Insert);
}

TEST(MotionState, test_insert_enter_inserts_newline)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("ab");
	buffer.SetCursorByte(1);
	motion.SetMode(Mode::Insert);

	EXPECT_EQ(motion.HandleKey(KeyCode::Enter, buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "a\nb");
	EXPECT_EQ(buffer.GetCursorByte(), 2u);
	EXPECT_EQ(motion.GetMode(), Mode::Insert);
}

TEST(MotionState, test_insert_backspace_erases_previous_codepoint)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("a\xC3\xA9" "b");
	buffer.SetCursorByte(3);
	motion.SetMode(Mode::Insert);

	EXPECT_EQ(motion.HandleKey(KeyCode::Backspace, buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "ab");
	EXPECT_EQ(buffer.GetCursorByte(), 1u);
	EXPECT_EQ(motion.GetMode(), Mode::Insert);

	buffer.SetCursorByte(0);
	EXPECT_EQ(motion.HandleKey(KeyCode::Backspace, buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetText(), "ab");
	EXPECT_EQ(buffer.GetCursorByte(), 0u);
}

TEST(MotionState, test_insert_escape_returns_to_normal)
{
	MotionState motion;
	Buffer buffer;
	buffer.SetText("abc");
	buffer.SetCursorByte(2);
	motion.SetMode(Mode::Insert);

	EXPECT_EQ(motion.HandleKey(KeyCode::Escape, buffer), KeyResult::Consumed);
	EXPECT_EQ(motion.GetMode(), Mode::Normal);
	EXPECT_EQ(buffer.GetText(), "abc");
	EXPECT_EQ(buffer.GetCursorByte(), 2u);

	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 2u);
	EXPECT_EQ(motion.HandleTextInput("g", buffer), KeyResult::Consumed);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);
}
