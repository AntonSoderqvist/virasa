#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <utility>

#include "virasa/editor/Buffer.h"

using namespace virasa::editor;

TEST(Buffer, test_buffer_owns_utf8_text_and_cursor_byte_offset)
{
	Buffer buffer;
	EXPECT_TRUE(buffer.GetText().empty());
	EXPECT_EQ(buffer.GetCursorByte(), 0u);

	buffer.SetText("A\xC3\xA9" "B");
	buffer.SetCursorByte(2u);
	EXPECT_EQ(buffer.GetCursorByte(), 1u);

	Buffer copied = buffer;
	EXPECT_EQ(copied.GetText(), std::string_view("A\xC3\xA9" "B", 4));
	EXPECT_EQ(copied.GetCursorByte(), 1u);

	buffer.InsertAtCursor("!");
	EXPECT_EQ(buffer.GetText(), std::string_view("A!\xC3\xA9" "B", 5));
	EXPECT_EQ(copied.GetText(), std::string_view("A\xC3\xA9" "B", 4));
	EXPECT_EQ(copied.GetCursorByte(), 1u);

	Buffer moved = std::move(copied);
	EXPECT_EQ(moved.GetText(), std::string_view("A\xC3\xA9" "B", 4));
	EXPECT_EQ(moved.GetCursorByte(), 1u);
	EXPECT_EQ(moved.PrevCodepointBoundary(2u), 1u);
	EXPECT_EQ(moved.NextCodepointBoundary(1u), 3u);
}

TEST(Buffer, test_clear_resets_text_and_cursor)
{
	Buffer buffer;
	buffer.SetText("alpha\nbeta");
	buffer.SetCursorByte(7u);

	buffer.Clear();

	EXPECT_TRUE(buffer.GetText().empty());
	EXPECT_EQ(buffer.GetCursorByte(), 0u);
	EXPECT_EQ(buffer.GetLineCount(), 1u);
	EXPECT_EQ(buffer.GetCursorLine(), 0u);
	EXPECT_EQ(buffer.GetCursorColumnByte(), 0u);

	buffer.InsertAtCursor("x");
	EXPECT_EQ(buffer.GetText(), std::string_view("x"));
	EXPECT_EQ(buffer.GetCursorByte(), 1u);
}

TEST(Buffer, test_set_text_replaces_text_and_clamps_cursor_to_zero)
{
	Buffer buffer;
	buffer.SetText("first");
	buffer.SetCursorByte(3u);

	const std::string textWithEmbeddedNull("A\0B\xFF", 4);
	buffer.SetText(std::string_view(textWithEmbeddedNull.data(), textWithEmbeddedNull.size()));

	EXPECT_EQ(buffer.GetText().size(), 4u);
	EXPECT_EQ(buffer.GetText(),
		std::string_view(textWithEmbeddedNull.data(), textWithEmbeddedNull.size()));
	EXPECT_EQ(static_cast<unsigned char>(buffer.GetText()[3]), 0xFFu);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);
}

TEST(Buffer, test_get_text_returns_internal_byte_view)
{
	Buffer buffer;
	buffer.SetText("hello");

	const Buffer& constBuffer = buffer;
	std::string_view view = constBuffer.GetText();
	ASSERT_EQ(view, std::string_view("hello"));
	EXPECT_EQ(view.data(), constBuffer.GetText().data());

	buffer.InsertAtCursor("!");
	EXPECT_EQ(buffer.GetText(), std::string_view("!hello"));
}

TEST(Buffer, test_get_cursor_byte_returns_current_offset)
{
	Buffer buffer;
	EXPECT_EQ(buffer.GetCursorByte(), 0u);

	buffer.SetText("abcdef");
	buffer.SetCursorByte(4u);
	EXPECT_EQ(buffer.GetCursorByte(), 4u);

	buffer.SetCursorByte(999u);
	EXPECT_EQ(buffer.GetCursorByte(), 6u);
}

TEST(Buffer, test_set_cursor_byte_clamps_and_snaps_to_codepoint_boundary)
{
	Buffer buffer;
	buffer.SetText("A\xC3\xA9" "B");

	buffer.SetCursorByte(0u);
	EXPECT_EQ(buffer.GetCursorByte(), 0u);

	buffer.SetCursorByte(2u);
	EXPECT_EQ(buffer.GetCursorByte(), 1u);

	buffer.SetCursorByte(3u);
	EXPECT_EQ(buffer.GetCursorByte(), 3u);

	buffer.SetCursorByte(100u);
	EXPECT_EQ(buffer.GetCursorByte(), 4u);
}

TEST(Buffer, test_insert_at_cursor_inserts_bytes_and_advances_cursor)
{
	Buffer buffer;
	buffer.SetText("abcd");
	buffer.SetCursorByte(2u);

	buffer.InsertAtCursor("XY");
	EXPECT_EQ(buffer.GetText(), std::string_view("abXYcd"));
	EXPECT_EQ(buffer.GetCursorByte(), 4u);

	buffer.InsertAtCursor("");
	EXPECT_EQ(buffer.GetText(), std::string_view("abXYcd"));
	EXPECT_EQ(buffer.GetCursorByte(), 4u);
}

TEST(Buffer, test_erase_range_removes_bytes_and_adjusts_cursor)
{
	Buffer buffer;
	buffer.SetText("A\xC3\xA9" "B\nCD");

	buffer.SetCursorByte(6u);
	buffer.EraseRange(1u, 4u);
	EXPECT_EQ(buffer.GetText(), std::string_view("A\nCD", 4));
	EXPECT_EQ(buffer.GetCursorByte(), 3u);

	buffer.SetText("abcdef");
	buffer.SetCursorByte(1u);
	buffer.EraseRange(4u, 2u);
	EXPECT_EQ(buffer.GetText(), std::string_view("abef"));
	EXPECT_EQ(buffer.GetCursorByte(), 1u);

	buffer.SetText("A\xC3\xA9" "B");
	buffer.SetCursorByte(4u);
	buffer.EraseRange(3u, 4u);
	EXPECT_EQ(buffer.GetText(), std::string_view("A\xC3\xA9", 3));
	EXPECT_EQ(buffer.GetCursorByte(), 3u);

	buffer.EraseRange(2u, 2u);
	EXPECT_EQ(buffer.GetText(), std::string_view("A\xC3\xA9", 3));
	EXPECT_EQ(buffer.GetCursorByte(), 3u);
}

TEST(Buffer, test_line_count_is_one_plus_newline_count)
{
	Buffer buffer;
	EXPECT_EQ(buffer.GetLineCount(), 1u);

	buffer.SetText("abc");
	EXPECT_EQ(buffer.GetLineCount(), 1u);

	buffer.SetText("\n");
	EXPECT_EQ(buffer.GetLineCount(), 2u);

	buffer.SetText("a\rb\nc\n");
	EXPECT_EQ(buffer.GetLineCount(), 3u);
}

TEST(Buffer, test_get_line_start_byte_returns_line_first_byte_offset)
{
	Buffer buffer;
	buffer.SetText("ab\ncd\n");

	EXPECT_EQ(buffer.GetLineStartByte(0u), 0u);
	EXPECT_EQ(buffer.GetLineStartByte(1u), 3u);
	EXPECT_EQ(buffer.GetLineStartByte(2u), 6u);
	EXPECT_EQ(buffer.GetLineStartByte(3u), 6u);
}

TEST(Buffer, test_get_line_end_byte_returns_line_terminator_or_end_offset)
{
	Buffer buffer;
	buffer.SetText("ab\ncd\n");

	EXPECT_EQ(buffer.GetLineEndByte(0u), 2u);
	EXPECT_EQ(buffer.GetLineEndByte(1u), 5u);
	EXPECT_EQ(buffer.GetLineEndByte(2u), 6u);
	EXPECT_EQ(buffer.GetLineEndByte(3u), 6u);

	EXPECT_EQ(buffer.GetText().substr(buffer.GetLineStartByte(0u),
			    buffer.GetLineEndByte(0u) - buffer.GetLineStartByte(0u)),
		std::string_view("ab"));
	EXPECT_EQ(buffer.GetText().substr(buffer.GetLineStartByte(1u),
			    buffer.GetLineEndByte(1u) - buffer.GetLineStartByte(1u)),
		std::string_view("cd"));
	EXPECT_EQ(buffer.GetText().substr(buffer.GetLineStartByte(2u),
			    buffer.GetLineEndByte(2u) - buffer.GetLineStartByte(2u)),
		std::string_view(""));
}

TEST(Buffer, test_cursor_line_is_index_of_line_containing_cursor)
{
	Buffer buffer;
	buffer.SetText("ab\ncd\n");

	buffer.SetCursorByte(0u);
	EXPECT_EQ(buffer.GetCursorLine(), 0u);

	buffer.SetCursorByte(3u);
	EXPECT_EQ(buffer.GetCursorLine(), 1u);

	buffer.SetCursorByte(5u);
	EXPECT_EQ(buffer.GetCursorLine(), 1u);

	buffer.SetCursorByte(6u);
	EXPECT_EQ(buffer.GetCursorLine(), 2u);
}

TEST(Buffer, test_cursor_column_byte_is_bytes_from_line_start)
{
	Buffer buffer;
	buffer.SetText("A\xC3\xA9\nBC");

	buffer.SetCursorByte(3u);
	EXPECT_EQ(buffer.GetCursorLine(), 0u);
	EXPECT_EQ(buffer.GetCursorColumnByte(), 3u);
	EXPECT_LE(buffer.GetCursorColumnByte(),
		buffer.GetLineEndByte(buffer.GetCursorLine()) -
			buffer.GetLineStartByte(buffer.GetCursorLine()));

	buffer.SetCursorByte(5u);
	EXPECT_EQ(buffer.GetCursorLine(), 1u);
	EXPECT_EQ(buffer.GetCursorColumnByte(), 1u);
	EXPECT_LE(buffer.GetCursorColumnByte(),
		buffer.GetLineEndByte(buffer.GetCursorLine()) -
			buffer.GetLineStartByte(buffer.GetCursorLine()));
}

TEST(Buffer, test_prev_codepoint_boundary_walks_back_to_codepoint_start)
{
	Buffer buffer;
	buffer.SetText("A\xC3\xA9" "B");

	EXPECT_EQ(buffer.PrevCodepointBoundary(0u), 0u);
	EXPECT_EQ(buffer.PrevCodepointBoundary(1u), 0u);
	EXPECT_EQ(buffer.PrevCodepointBoundary(2u), 1u);
	EXPECT_EQ(buffer.PrevCodepointBoundary(3u), 1u);
	EXPECT_EQ(buffer.PrevCodepointBoundary(4u), 3u);
	EXPECT_EQ(buffer.PrevCodepointBoundary(99u), 3u);
	EXPECT_EQ(buffer.GetText(), std::string_view("A\xC3\xA9" "B", 4));
}

TEST(Buffer, test_next_codepoint_boundary_walks_forward_to_codepoint_start)
{
	Buffer buffer;
	buffer.SetText("A\xC3\xA9" "B");

	EXPECT_EQ(buffer.NextCodepointBoundary(0u), 1u);
	EXPECT_EQ(buffer.NextCodepointBoundary(1u), 3u);
	EXPECT_EQ(buffer.NextCodepointBoundary(2u), 3u);
	EXPECT_EQ(buffer.NextCodepointBoundary(3u), 4u);
	EXPECT_EQ(buffer.NextCodepointBoundary(4u), 4u);
	EXPECT_EQ(buffer.NextCodepointBoundary(99u), 4u);
	EXPECT_EQ(buffer.GetText(), std::string_view("A\xC3\xA9" "B", 4));
}
