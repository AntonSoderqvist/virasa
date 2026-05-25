#ifndef VIRASA_EDITOR_BUFFER_H
#define VIRASA_EDITOR_BUFFER_H

#include <cstddef>
#include <string>
#include <string_view>

namespace virasa::editor
{

/**
 * @brief Owns editable UTF-8 text bytes and a cursor byte offset.
 */
class Buffer final
{
	public:
	Buffer() = default;
	Buffer(const Buffer&) = default;
	Buffer(Buffer&&) = default;
	Buffer& operator=(const Buffer&) = default;
	Buffer& operator=(Buffer&&) = default;
	~Buffer() = default;

	/**
	 * @brief Clear all text and reset the cursor to the start.
	 */
	void Clear();

	/**
	 * @brief Replace the buffer contents with the supplied bytes.
	 * @param text Text bytes to copy into the buffer.
	 */
	void SetText(std::string_view text);

	/**
	 * @brief Get a view of the internal text bytes.
	 * @return A string view over the internal text storage.
	 */
	[[nodiscard]] std::string_view GetText() const noexcept;

	/**
	 * @brief Get the current cursor byte offset.
	 * @return The cursor byte offset in the range [0, GetText().size()].
	 */
	[[nodiscard]] std::size_t GetCursorByte() const noexcept;

	/**
	 * @brief Set the cursor byte offset, clamped and snapped to a codepoint boundary.
	 * @param byteIndex Requested cursor byte offset.
	 */
	void SetCursorByte(std::size_t byteIndex);

	/**
	 * @brief Insert bytes at the current cursor position.
	 * @param text Text bytes to insert.
	 */
	void InsertAtCursor(std::string_view text);

	/**
	 * @brief Erase a half-open byte range from the buffer.
	 * @param beginByte Requested beginning byte offset.
	 * @param endByte Requested ending byte offset.
	 */
	void EraseRange(std::size_t beginByte, std::size_t endByte);

	/**
	 * @brief Count lines separated by '\n' bytes.
	 * @return One plus the number of '\n' bytes in the buffer.
	 */
	[[nodiscard]] std::size_t GetLineCount() const noexcept;

	/**
	 * @brief Get the starting byte offset of a line.
	 * @param lineIndex Zero-based line index.
	 * @return The first byte offset of the requested line, or GetText().size() if out of range.
	 */
	[[nodiscard]] std::size_t GetLineStartByte(std::size_t lineIndex) const;

	/**
	 * @brief Get the ending byte offset of a line.
	 * @param lineIndex Zero-based line index.
	 * @return The exclusive end byte offset of the requested line, or GetText().size() if out of
	 * range.
	 */
	[[nodiscard]] std::size_t GetLineEndByte(std::size_t lineIndex) const;

	/**
	 * @brief Get the zero-based line index containing the cursor.
	 * @return The line index containing the cursor.
	 */
	[[nodiscard]] std::size_t GetCursorLine() const noexcept;

	/**
	 * @brief Get the cursor column measured in bytes from the start of its line.
	 * @return The cursor column in bytes.
	 */
	[[nodiscard]] std::size_t GetCursorColumnByte() const noexcept;

	/**
	 * @brief Find the previous codepoint boundary before a byte offset.
	 * @param byteIndex Reference byte offset.
	 * @return The nearest codepoint boundary strictly before the clamped byte offset.
	 */
	[[nodiscard]] std::size_t PrevCodepointBoundary(std::size_t byteIndex) const noexcept;

	/**
	 * @brief Find the next codepoint boundary after a byte offset.
	 * @param byteIndex Reference byte offset.
	 * @return The nearest codepoint boundary strictly after the clamped byte offset.
	 */
	[[nodiscard]] std::size_t NextCodepointBoundary(std::size_t byteIndex) const noexcept;

	private:
	std::string _text = {};
	std::size_t _cursorByte = 0u;
};

} // namespace virasa::editor

#endif // VIRASA_EDITOR_BUFFER_H
