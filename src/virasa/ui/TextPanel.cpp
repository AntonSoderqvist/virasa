#include "virasa/ui/TextPanel.h"

#include <string>

namespace virasa
{
namespace ui
{

void TextPanel::SetConfig(const TextPanelConfig& config) noexcept
{
	_config = config;
}

const TextPanelConfig& TextPanel::GetConfig() const noexcept
{
	return _config;
}

void TextPanel::Render(DrawList& out, std::string_view text, float x, float y, float width,
	float height, const FontAtlas& atlas) const
{
	out.AddQuad(QuadCommand{x, y, width, height, _config.background});

	const float lineHeight = atlas.GetAscender() - atlas.GetDescender();
	const float advance = lineHeight * _config.lineSpacing;
	const float textX = x + _config.paddingX;
	const float baseY = y + _config.paddingY + atlas.GetAscender();

	std::string currentLine;
	currentLine.reserve(text.size());

	std::size_t lineIndex = 0;

	const auto emitLine = [&](std::size_t currentLineIndex)
	{
		if (!currentLine.empty())
		{
			out.AddText(textX,
				baseY + static_cast<float>(currentLineIndex) * advance,
				std::string_view(currentLine),
				_config.foreground);
		}
	};

	for (const char ch : text)
	{
		if (ch == '\r')
		{
			continue;
		}

		if (ch == '\n')
		{
			emitLine(lineIndex);
			currentLine.clear();
			++lineIndex;
			continue;
		}

		currentLine.push_back(ch);
	}

	emitLine(lineIndex);
}

} // namespace ui
} // namespace virasa
