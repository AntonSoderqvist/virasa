#ifndef VIRASA_UI_FONTATLAS_H
#define VIRASA_UI_FONTATLAS_H

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace virasa::ui
{
/**
 * @brief Error codes returned by FontAtlas initialization.
 */
enum class FontAtlasError : uint32_t
{
	None = 0,
	FileNotFound,
	FreeTypeInitFailed,
	FaceLoadFailed,
	AtlasTooSmall
};

/**
 * @brief Metrics and atlas coordinates for a baked glyph.
 */
struct GlyphMetrics
{
	public:
	float advance = 0.0f;
	int16_t bearingX = 0;
	int16_t bearingY = 0;
	uint16_t width = 0u;
	uint16_t height = 0u;
	uint16_t u0 = 0u;
	uint16_t v0 = 0u;
	uint16_t u1 = 0u;
	uint16_t v1 = 0u;
};

/**
 * @brief Bakes an ASCII font atlas and exposes glyph metrics and bitmap data.
 */
class FontAtlas final
{
	public:
	FontAtlas() = default;
	FontAtlas(const FontAtlas&) = delete;
	FontAtlas& operator=(const FontAtlas&) = delete;
	FontAtlas(FontAtlas&& other) noexcept;
	FontAtlas& operator=(FontAtlas&& other) noexcept;
	~FontAtlas() = default;

	/**
	 * @brief Initialize the atlas from a TrueType font file.
	 * @param ttfPath Path to the .ttf file.
	 * @param pixelSize Requested pixel height passed to FreeType.
	 * @return FontAtlasError::None on success, otherwise a specific failure code.
	 */
	[[nodiscard]] FontAtlasError Initialize(std::string_view ttfPath, uint32_t pixelSize);

	/**
	 * @brief Get metrics for a codepoint or the missing-glyph fallback.
	 * @param codepoint Unicode codepoint to query.
	 * @return Const reference to baked glyph metrics or the fallback metrics.
	 */
	[[nodiscard]] const GlyphMetrics& GetGlyph(uint32_t codepoint) const noexcept;

	/**
	 * @brief Check whether a codepoint was baked into the atlas.
	 * @param codepoint Unicode codepoint to query.
	 * @return True if the glyph exists in the baked ASCII range.
	 */
	[[nodiscard]] bool HasGlyph(uint32_t codepoint) const noexcept;

	/**
	 * @brief Get the atlas bitmap as an 8-bit grayscale span.
	 * @return Span over row-major atlas pixels.
	 */
	[[nodiscard]] std::span<const uint8_t> GetBitmap() const noexcept;

	/**
	 * @brief Get the atlas width in pixels.
	 * @return Atlas width in pixels.
	 */
	[[nodiscard]] uint32_t GetAtlasWidth() const noexcept;

	/**
	 * @brief Get the atlas height in pixels.
	 * @return Atlas height in pixels.
	 */
	[[nodiscard]] uint32_t GetAtlasHeight() const noexcept;

	/**
	 * @brief Get the configured FreeType pixel height.
	 * @return Pixel size passed to Initialize, or 0 if uninitialized.
	 */
	[[nodiscard]] uint32_t GetPixelSize() const noexcept;

	/**
	 * @brief Get the recommended line height in pixels.
	 * @return Baseline-to-baseline distance in pixels.
	 */
	[[nodiscard]] float GetLineHeight() const noexcept;

	/**
	 * @brief Get the typographic ascender in pixels.
	 * @return Ascender distance above the baseline.
	 */
	[[nodiscard]] float GetAscender() const noexcept;

	/**
	 * @brief Get the typographic descender in pixels.
	 * @return Descender distance below the baseline.
	 */
	[[nodiscard]] float GetDescender() const noexcept;

	private:
	std::vector<uint8_t> _bitmap = {};
	std::vector<GlyphMetrics> _glyphs = {};
	GlyphMetrics _missing = {};
	uint32_t _atlasWidth = 0u;
	uint32_t _atlasHeight = 0u;
	uint32_t _pixelSize = 0u;
	float _lineHeight = 0.0f;
	float _ascender = 0.0f;
	float _descender = 0.0f;
	bool _initialized = false;
};
} // namespace virasa::ui

#endif
