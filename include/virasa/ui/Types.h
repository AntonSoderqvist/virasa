#ifndef VIRASA_UI_TYPES_H
#define VIRASA_UI_TYPES_H

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace virasa
{
namespace ui
{

/**
 * @brief Identifies the kind of draw command.
 */
enum class DrawCommandKind : uint32_t
{
	Quad,
	Text,
	ImageQuad
};

/**
 * @brief Linear, non-premultiplied RGBA color.
 */
struct Color
{
	public:
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	float a = 1.0f;
};

/**
 * @brief Describes one filled axis-aligned rectangle in framebuffer-pixel space.
 */
struct QuadCommand
{
	public:
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	virasa::ui::Color color;
};

/**
 * @brief Describes one text run referencing bytes in a DrawList text buffer.
 */
struct TextCommand
{
	public:
	float x = 0.0f;
	float y = 0.0f;
	uint32_t textOffset = 0u;
	uint32_t textLength = 0u;
	virasa::ui::Color color;
};

/**
 * @brief Describes one textured axis-aligned rectangle in framebuffer-pixel space.
 */
struct ImageQuadCommand
{
	public:
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	float u0 = 0.0f;
	float v0 = 0.0f;
	float u1 = 1.0f;
	float v1 = 1.0f;
	uint32_t textureSlot = 0u;
	virasa::ui::Color tint = {1.0f, 1.0f, 1.0f, 1.0f};
};

/**
 * @brief Collects quad, image-quad, and text draw commands for a UI frame.
 */
class DrawList final
{
	public:
	/**
	 * @brief Reset the draw list to its default-constructed observable state.
	 */
	void Clear()
	{
		_quads.clear();
		_texts.clear();
		_imageQuads.clear();
		_textBuffer.clear();
	}

	/**
	 * @brief Append a quad command.
	 * @param quad Quad command to append.
	 */
	void AddQuad(const QuadCommand& quad)
	{
		_quads.push_back(quad);
	}

	/**
	 * @brief Append a text command and copy its UTF-8 bytes into the text buffer.
	 * @param x Text pen x position in framebuffer pixels.
	 * @param y Text pen y position in framebuffer pixels.
	 * @param text UTF-8 text bytes to append.
	 * @param color Text color.
	 */
	void AddText(float x, float y, std::string_view text, virasa::ui::Color color)
	{
		const uint32_t textOffset = static_cast<uint32_t>(_textBuffer.size());
		const uint32_t textLength = static_cast<uint32_t>(text.size());
		_textBuffer.append(text.data(), text.size());
		_texts.push_back(TextCommand{x, y, textOffset, textLength, color});
	}

	/**
	 * @brief Append an image quad command.
	 * @param quad Image quad command to append.
	 */
	void AddImageQuad(const ImageQuadCommand& quad)
	{
		_imageQuads.push_back(quad);
	}

	/**
	 * @brief Get the appended quad commands.
	 * @return A span over the internal quad storage.
	 */
	[[nodiscard]] std::span<const QuadCommand> GetQuads() const noexcept
	{
		return std::span<const QuadCommand>(_quads.data(), _quads.size());
	}

	/**
	 * @brief Get the appended text commands.
	 * @return A span over the internal text-command storage.
	 */
	[[nodiscard]] std::span<const TextCommand> GetTexts() const noexcept
	{
		return std::span<const TextCommand>(_texts.data(), _texts.size());
	}

	/**
	 * @brief Get the appended image quad commands.
	 * @return A span over the internal image-quad storage.
	 */
	[[nodiscard]] std::span<const ImageQuadCommand> GetImageQuads() const noexcept
	{
		return std::span<const ImageQuadCommand>(_imageQuads.data(), _imageQuads.size());
	}

	/**
	 * @brief Get the accumulated text buffer.
	 * @return A string view into the internal text buffer.
	 */
	[[nodiscard]] std::string_view GetTextBuffer() const noexcept
	{
		return std::string_view(_textBuffer);
	}

	private:
	std::vector<QuadCommand> _quads = {};
	std::vector<TextCommand> _texts = {};
	std::vector<ImageQuadCommand> _imageQuads = {};
	std::string _textBuffer = {};
};

} // namespace ui
} // namespace virasa

#endif