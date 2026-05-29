#include "virasa/editor/io/ImageLoader.h"

#include "virasa/core/Logger.h"

#include <quill/Logger.h>
#include <quill/LogMacros.h>

#include <fstream>
#include <iterator>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace virasa::editor::io
{

DecodedImage LoadFromMemory(std::span<const std::byte> bytes)
{
    // Step 1: validate input
    if (bytes.empty() || bytes.data() == nullptr)
    {
        DecodedImage result;
        result.error    = ImageLoadError::DecodeFailed;
        result.width    = 0;
        result.height   = 0;
        result.channels = 4;
        return result;
    }

    // Step 2: decode via stb_image, forcing 4 channels (RGBA)
    int w = 0;
    int h = 0;
    int srcChannels = 0;
    stbi_uc* data = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(bytes.data()),
        static_cast<int>(bytes.size()),
        &w, &h, &srcChannels,
        4
    );

    if (data == nullptr)
    {
        quill::Logger* logger = virasa::Logger::GetLogger("editor.io");
        LOG_ERROR(logger, "stb_image failed to decode image: {}", stbi_failure_reason());

        DecodedImage result;
        result.error    = ImageLoadError::DecodeFailed;
        result.width    = 0;
        result.height   = 0;
        result.channels = 4;
        return result;
    }

    // Step 3: copy pixel data into owned vector, free stb allocation
    const std::size_t byteCount = static_cast<std::size_t>(w) *
                                  static_cast<std::size_t>(h) * 4u;

    DecodedImage result;
    result.pixels.resize(byteCount);
    std::memcpy(result.pixels.data(), data, byteCount);
    stbi_image_free(data);

    result.width    = static_cast<uint32_t>(w);
    result.height   = static_cast<uint32_t>(h);
    result.channels = 4u;
    result.error    = ImageLoadError::None;

    return result;
}

DecodedImage LoadFromFile(const std::string& path)
{
    // Step 1: attempt to open the file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        quill::Logger* logger = virasa::Logger::GetLogger("editor.io");
        LOG_ERROR(logger, "ImageLoader: failed to open file: {}", path);

        DecodedImage result;
        result.error    = ImageLoadError::FileNotFound;
        result.width    = 0;
        result.height   = 0;
        result.channels = 4;
        return result;
    }

    // Step 2: determine file size and read contents
    const std::streamsize fileSize = file.tellg();
    if (fileSize <= 0)
    {
        // Empty file treated as FileNotFound
        DecodedImage result;
        result.error    = ImageLoadError::FileNotFound;
        result.width    = 0;
        result.height   = 0;
        result.channels = 4;
        return result;
    }

    file.seekg(0, std::ios::beg);
    std::vector<std::byte> fileBytes(static_cast<std::size_t>(fileSize));
    file.read(reinterpret_cast<char*>(fileBytes.data()), fileSize);

    // Step 3: delegate to LoadFromMemory
    return LoadFromMemory(std::span<const std::byte>(fileBytes));
}

} // namespace virasa::editor::io
