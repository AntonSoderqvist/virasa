#ifndef VIRASA_EDITOR_IO_IMAGE_LOADER_H
#define VIRASA_EDITOR_IO_IMAGE_LOADER_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace virasa::editor::io
{

/**
 * @brief Error codes returned by image loading operations.
 *
 * Underlying type is uint8_t. Values are a closed set in this contract version;
 * future versions may append new values without reordering existing ones.
 */
enum class ImageLoadError : uint8_t
{
    None             = 0, ///< Decode succeeded.
    FileNotFound,         ///< File could not be opened or was empty.
    DecodeFailed,         ///< Bytes were not a recognized image or decoding aborted.
    UnsupportedFormat,    ///< Reserved for future use.
};

/**
 * @brief Holds the result of a successful or failed image-decode call.
 *
 * DecodedImage owns its pixel storage by value. It is non-copyable to avoid
 * accidentally duplicating multi-megabyte texture data, and movable so that
 * producers may std::move it into downstream consumers.
 *
 * On success: error == ImageLoadError::None, width > 0, height > 0,
 * channels == 4, and pixels.size() == width * height * 4.
 * On failure: error != ImageLoadError::None, width == 0, height == 0,
 * pixels is empty.
 */
struct DecodedImage
{
public:
    // Non-copyable
    DecodedImage(const DecodedImage&)            = delete;
    DecodedImage& operator=(const DecodedImage&) = delete;

    // Movable
    DecodedImage(DecodedImage&&)            = default;
    DecodedImage& operator=(DecodedImage&&) = default;

    // Default constructible
    DecodedImage() = default;

public:
    /// @brief Decoded pixel data in tightly-packed RGBA8 scanline order, top row first.
    std::vector<std::byte> pixels;

    /// @brief Width of the decoded image in texels.
    uint32_t width = 0u;

    /// @brief Height of the decoded image in texels.
    uint32_t height = 0u;

    /// @brief Number of channels per texel (always 4 in this contract version).
    uint32_t channels = 4u;

    /// @brief Error status of the decode operation.
    virasa::editor::io::ImageLoadError error = virasa::editor::io::ImageLoadError::None;
};

/**
 * @brief Decode an image from an in-memory byte span.
 *
 * Performs pure CPU computation with no Vulkan API calls, no GPU resource
 * allocation, and no file-system I/O. Safe to call on any thread.
 *
 * @param bytes Complete file contents of a single supported image
 *              (PNG, JPEG, BMP, TGA, or any format recognized by stb_image).
 * @return DecodedImage with error == ImageLoadError::None on success,
 *         or error == ImageLoadError::DecodeFailed on failure.
 */
[[nodiscard]] virasa::editor::io::DecodedImage LoadFromMemory(std::span<const std::byte> bytes);

/**
 * @brief Load and decode an image from a file path.
 *
 * Opens the file at @p path, reads its complete contents, and delegates to
 * LoadFromMemory. Blocking; call from a background thread in latency-sensitive
 * contexts.
 *
 * @param path Filesystem path to the image file.
 * @return DecodedImage with error == ImageLoadError::None on success,
 *         error == ImageLoadError::FileNotFound if the file could not be opened
 *         or is empty, or error == ImageLoadError::DecodeFailed on decoder failure.
 */
[[nodiscard]] virasa::editor::io::DecodedImage LoadFromFile(const std::string& path);

} // namespace virasa::editor::io

#endif // VIRASA_EDITOR_IO_IMAGE_LOADER_H
