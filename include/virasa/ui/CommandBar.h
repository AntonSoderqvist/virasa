#ifndef VIRASA_UI_COMMANDBAR_H
#define VIRASA_UI_COMMANDBAR_H

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
 * @brief Configuration for a command bar overlay.
 */
struct CommandBarConfig
{
	public:
	virasa::ui::Color background = {0.039f, 0.165f, 0.200f, 0.7f};
	virasa::ui::Color foreground = {0.9f, 0.9f, 0.9f, 1.0f};
	virasa::ui::Color cursorColor = {0.9f, 0.9f, 0.9f, 1.0f};
	float paddingX = 6.0f;
	float paddingY = 4.0f;
};

/**
 * @brief Produces draw-list commands for a bottom-of-window command bar.
 */
class CommandBar final
{
	public:
	/**
	 * @brief Replace the stored command bar configuration.
	 * @param config Configuration to copy into the command bar.
	 */
	void SetConfig(const CommandBarConfig& config) noexcept;

	/**
	 * @brief Get the current command bar configuration.
	 * @return Const reference to the stored configuration.
	 */
	[[nodiscard]] const CommandBarConfig& GetConfig() const noexcept;

	/**
	 * @brief Append background, text, and cursor commands to a draw list.
	 * @param out Draw list receiving the appended commands.
	 * @param text UTF-8 text to display.
	 * @param cursorByteIndex Cursor position in the UTF-8 byte buffer.
	 * @param atlas Font atlas providing glyph metrics.
	 * @param windowWidth Window width in framebuffer pixels.
	 * @param windowHeight Window height in framebuffer pixels.
	 */
	void Render(virasa::ui::DrawList& out, std::string_view text, std::size_t cursorByteIndex,
		const virasa::ui::FontAtlas& atlas, uint32_t windowWidth, uint32_t windowHeight) const;

	private:
	CommandBarConfig _config = {};
};

} // namespace ui
} // namespace virasa

#endif
