#include <gtest/gtest.h>

#include "virasa/editor/io/ImageLoader.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace virasa::editor::io;

namespace
{

constexpr std::array<unsigned char, 68> kPng1x1Rgba = {
	0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
	0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
	0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
	0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4,
	0x89, 0x00, 0x00, 0x00, 0x0B, 0x49, 0x44, 0x41,
	0x54, 0x78, 0xDA, 0x63, 0xF8, 0x0F, 0x04, 0x00,
	0x09, 0xFB, 0x03, 0xFD, 0x68, 0xFA, 0x1C, 0xCC,
	0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44,
	0xAE, 0x42, 0x60, 0x82
};

std::vector<std::byte> ToBytes(const std::array<unsigned char, 68>& source)
{
	std::vector<std::byte> bytes;
	bytes.reserve(source.size());
	for (unsigned char value : source)
	{
		bytes.push_back(static_cast<std::byte>(value));
	}
	return bytes;
}

std::filesystem::path MakeUniquePath(const std::string& stem)
{
	const auto unique = std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
		"_" + std::to_string(std::filesystem::file_time_type::clock::now().time_since_epoch().count());
	return std::filesystem::temp_directory_path() / (stem + "_" + unique);
}

void WriteBytesToFile(const std::filesystem::path& path, const std::vector<std::byte>& bytes)
{
	std::ofstream stream(path, std::ios::binary);
	ASSERT_TRUE(stream.is_open());
	stream.write(reinterpret_cast<const char*>(bytes.data()),
		static_cast<std::streamsize>(bytes.size()));
	ASSERT_TRUE(stream.good());
}

} // namespace

TEST(ImageLoader, test_image_load_error_enum_values_in_declared_order)
{
	static_assert(std::is_same_v<std::underlying_type_t<ImageLoadError>, uint8_t>);

	EXPECT_EQ(static_cast<uint8_t>(ImageLoadError::None), static_cast<uint8_t>(0));
	EXPECT_EQ(static_cast<uint8_t>(ImageLoadError::FileNotFound), static_cast<uint8_t>(1));
	EXPECT_EQ(static_cast<uint8_t>(ImageLoadError::DecodeFailed), static_cast<uint8_t>(2));
	EXPECT_EQ(static_cast<uint8_t>(ImageLoadError::UnsupportedFormat), static_cast<uint8_t>(3));
}

TEST(ImageLoader, test_decoded_image_holds_owned_rgba8_pixels)
{
	static_assert(std::is_default_constructible_v<DecodedImage>);
	static_assert(!std::is_copy_constructible_v<DecodedImage>);
	static_assert(!std::is_copy_assignable_v<DecodedImage>);
	static_assert(std::is_move_constructible_v<DecodedImage>);
	static_assert(std::is_move_assignable_v<DecodedImage>);

	DecodedImage image;
	EXPECT_TRUE(image.pixels.empty());
	EXPECT_EQ(image.width, 0u);
	EXPECT_EQ(image.height, 0u);
	EXPECT_EQ(image.channels, 4u);
	EXPECT_EQ(image.error, ImageLoadError::None);

	const auto pngBytes = ToBytes(kPng1x1Rgba);
	DecodedImage decoded = LoadFromMemory(std::span<const std::byte>(pngBytes.data(), pngBytes.size()));
	ASSERT_EQ(decoded.error, ImageLoadError::None);
	EXPECT_GT(decoded.width, 0u);
	EXPECT_GT(decoded.height, 0u);
	EXPECT_EQ(decoded.channels, 4u);
	EXPECT_EQ(decoded.pixels.size(),
		static_cast<std::size_t>(decoded.width) * decoded.height * decoded.channels);
}

TEST(ImageLoader, test_load_from_memory_decodes_supported_image_formats)
{
	DecodedImage emptyResult = LoadFromMemory(std::span<const std::byte>());
	EXPECT_EQ(emptyResult.error, ImageLoadError::DecodeFailed);
	EXPECT_EQ(emptyResult.width, 0u);
	EXPECT_EQ(emptyResult.height, 0u);
	EXPECT_EQ(emptyResult.channels, 4u);
	EXPECT_TRUE(emptyResult.pixels.empty());

	const std::vector<std::byte> invalidBytes = {
		std::byte{0x00}, std::byte{0x11}, std::byte{0x22}, std::byte{0x33}
	};
	DecodedImage invalidResult =
		LoadFromMemory(std::span<const std::byte>(invalidBytes.data(), invalidBytes.size()));
	EXPECT_EQ(invalidResult.error, ImageLoadError::DecodeFailed);
	EXPECT_EQ(invalidResult.width, 0u);
	EXPECT_EQ(invalidResult.height, 0u);
	EXPECT_EQ(invalidResult.channels, 4u);
	EXPECT_TRUE(invalidResult.pixels.empty());

	const auto pngBytes = ToBytes(kPng1x1Rgba);
	DecodedImage decoded = LoadFromMemory(std::span<const std::byte>(pngBytes.data(), pngBytes.size()));
	ASSERT_EQ(decoded.error, ImageLoadError::None);
	EXPECT_EQ(decoded.width, 1u);
	EXPECT_EQ(decoded.height, 1u);
	EXPECT_EQ(decoded.channels, 4u);
	ASSERT_EQ(decoded.pixels.size(), 4u);
	EXPECT_EQ(std::to_integer<unsigned int>(decoded.pixels[0]), 255u);
	EXPECT_EQ(std::to_integer<unsigned int>(decoded.pixels[1]), 255u);
	EXPECT_EQ(std::to_integer<unsigned int>(decoded.pixels[2]), 255u);
	EXPECT_EQ(std::to_integer<unsigned int>(decoded.pixels[3]), 255u);
}

TEST(ImageLoader, test_load_from_file_reads_file_and_delegates)
{
	const auto missingPath = MakeUniquePath("imageloader_missing.png");
	DecodedImage missing = LoadFromFile(missingPath.string());
	EXPECT_EQ(missing.error, ImageLoadError::FileNotFound);
	EXPECT_EQ(missing.width, 0u);
	EXPECT_EQ(missing.height, 0u);
	EXPECT_EQ(missing.channels, 4u);
	EXPECT_TRUE(missing.pixels.empty());

	const auto emptyPath = MakeUniquePath("imageloader_empty.png");
	{
		std::ofstream emptyFile(emptyPath, std::ios::binary);
		ASSERT_TRUE(emptyFile.is_open());
	}
	DecodedImage empty = LoadFromFile(emptyPath.string());
	EXPECT_EQ(empty.error, ImageLoadError::FileNotFound);
	EXPECT_EQ(empty.width, 0u);
	EXPECT_EQ(empty.height, 0u);
	EXPECT_EQ(empty.channels, 4u);
	EXPECT_TRUE(empty.pixels.empty());
	std::filesystem::remove(emptyPath);

	const auto invalidPath = MakeUniquePath("imageloader_invalid.bin");
	const std::vector<std::byte> invalidBytes = {
		std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}
	};
	WriteBytesToFile(invalidPath, invalidBytes);
	DecodedImage invalid = LoadFromFile(invalidPath.string());
	EXPECT_EQ(invalid.error, ImageLoadError::DecodeFailed);
	EXPECT_EQ(invalid.width, 0u);
	EXPECT_EQ(invalid.height, 0u);
	EXPECT_EQ(invalid.channels, 4u);
	EXPECT_TRUE(invalid.pixels.empty());
	std::filesystem::remove(invalidPath);

	const auto validPath = MakeUniquePath("imageloader_valid.png");
	const auto pngBytes = ToBytes(kPng1x1Rgba);
	WriteBytesToFile(validPath, pngBytes);
	DecodedImage fromFile = LoadFromFile(validPath.string());
	DecodedImage fromMemory = LoadFromMemory(std::span<const std::byte>(pngBytes.data(), pngBytes.size()));
	ASSERT_EQ(fromFile.error, ImageLoadError::None);
	ASSERT_EQ(fromMemory.error, ImageLoadError::None);
	EXPECT_EQ(fromFile.width, fromMemory.width);
	EXPECT_EQ(fromFile.height, fromMemory.height);
	EXPECT_EQ(fromFile.channels, fromMemory.channels);
	EXPECT_EQ(fromFile.pixels, fromMemory.pixels);
	std::filesystem::remove(validPath);
}