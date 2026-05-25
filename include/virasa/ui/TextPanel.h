#ifndef VIRASA_UI_TEXTPANEL_H
#define VIRASA_UI_TEXTPANEL_H

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/Types.h"

namespace virasa
{
namespace ui
{

/**
 * @brief Cursor rendering style for a TextPanel.
 */
enum class CursorStyle : uint8_t
{
	None,
	Block,
	Insertion
};

/**
 * @brief Configuration for a text panel's appearance.
 */
struct TextPanelConfig
{
public:
	virasa::ui::Color background = {0.078f, 0.082f, 0.094f, 1.0f};
	virasa::ui::Color foreground = {0.9f, 0.9f, 0.9f, 1.0f};
	virasa::ui::Color cursor = {0.9f, 0.9f, 0.9f, 1.0f};
	float paddingX = 6.0f;
	float paddingY = 4.0f;
	float lineSpacing = 1.0f;
	float cursorBarWidth = 2.0f;
};

/**
 * @brief Renders a configured background panel and multiline text into a DrawList.
 */
class TextPanel final
{
public:
	/**
	 * @brief Replace the stored panel configuration.
	 * @param config Configuration to copy into the panel.
	 */
	void SetConfig(const TextPanelConfig& config) noexcept;

	/**
	 * @brief Access the stored panel configuration.
	 * @return Const reference to the current configuration.
	 */
	[[nodiscard]] const TextPanelConfig& GetConfig() const noexcept;

	/**
	 * @brief Append the panel background, text commands, and optional cursor quad to a draw list.
	 * @param out Draw list receiving the generated commands.
	 * @param text Source text to split into logical lines.
	 * @param x Panel left position in framebuffer pixels.
	 * @param y Panel top position in framebuffer pixels.
	 * @param width Panel width in framebuffer pixels.
	 * @param height Panel height in framebuffer pixels.
	 * @param atlas Font atlas providing glyph metrics and typographic values.
	 * @param cursorStyle Style of cursor to render; CursorStyle::None suppresses the cursor quad.
	 * @param cursorByte Byte offset within text at which the cursor is placed.
	 */
	void Render(
		virasa::ui::DrawList& out,
		std::string_view text,
		float x,
		float y,
		float width,
		float height,
		const virasa::ui::FontAtlas& atlas,
		CursorStyle cursorStyle,
		std::size_t cursorByte) const;

private:
	TextPanelConfig _config = {};
};

}
}

#endif
