#include <cstdint>
#include <gtest/gtest.h>
#include <sstream>

#include "virasa/window/Types.h"

using namespace virasa;

TEST(Types, test_error_code_enum_values_in_declared_order)
{
	// None must be explicitly 0
	EXPECT_EQ(static_cast<uint8_t>(ErrorCode::None), 0u);
	EXPECT_EQ(static_cast<uint8_t>(ErrorCode::SdlInitFailed), 1u);
	EXPECT_EQ(static_cast<uint8_t>(ErrorCode::WindowCreateFailed), 2u);
	EXPECT_EQ(static_cast<uint8_t>(ErrorCode::SurfaceCreateFailed), 3u);
	EXPECT_EQ(static_cast<uint8_t>(ErrorCode::AlreadyInitialized), 4u);
	EXPECT_EQ(static_cast<uint8_t>(ErrorCode::NotInitialized), 5u);

	// All values must be distinct
	EXPECT_NE(ErrorCode::None, ErrorCode::SdlInitFailed);
	EXPECT_NE(ErrorCode::None, ErrorCode::WindowCreateFailed);
	EXPECT_NE(ErrorCode::None, ErrorCode::SurfaceCreateFailed);
	EXPECT_NE(ErrorCode::None, ErrorCode::AlreadyInitialized);
	EXPECT_NE(ErrorCode::None, ErrorCode::NotInitialized);
	EXPECT_NE(ErrorCode::SdlInitFailed, ErrorCode::WindowCreateFailed);
	EXPECT_NE(ErrorCode::SdlInitFailed, ErrorCode::SurfaceCreateFailed);
	EXPECT_NE(ErrorCode::SdlInitFailed, ErrorCode::AlreadyInitialized);
	EXPECT_NE(ErrorCode::SdlInitFailed, ErrorCode::NotInitialized);
	EXPECT_NE(ErrorCode::WindowCreateFailed, ErrorCode::SurfaceCreateFailed);
	EXPECT_NE(ErrorCode::WindowCreateFailed, ErrorCode::AlreadyInitialized);
	EXPECT_NE(ErrorCode::WindowCreateFailed, ErrorCode::NotInitialized);
	EXPECT_NE(ErrorCode::SurfaceCreateFailed, ErrorCode::AlreadyInitialized);
	EXPECT_NE(ErrorCode::SurfaceCreateFailed, ErrorCode::NotInitialized);
	EXPECT_NE(ErrorCode::AlreadyInitialized, ErrorCode::NotInitialized);
}

TEST(Types, test_size_2d_holds_unsigned_pixel_dimensions)
{
	// Default construction yields width == 0, height == 0
	Size2D defaultSize;
	EXPECT_EQ(defaultSize.width, 0u);
	EXPECT_EQ(defaultSize.height, 0u);

	// Members are assignable and hold the assigned values
	Size2D s;
	s.width = 1920u;
	s.height = 1080u;
	EXPECT_EQ(s.width, 1920u);
	EXPECT_EQ(s.height, 1080u);

	// Copyable
	Size2D copy = s;
	EXPECT_EQ(copy.width, 1920u);
	EXPECT_EQ(copy.height, 1080u);

	// Movable
	Size2D moved = std::move(copy);
	EXPECT_EQ(moved.width, 1920u);
	EXPECT_EQ(moved.height, 1080u);

	// width and height are uint32_t
	static_assert(std::is_same_v<decltype(s.width), uint32_t>, "width must be uint32_t");
	static_assert(std::is_same_v<decltype(s.height), uint32_t>, "height must be uint32_t");
}

TEST(Types, test_error_code_has_ostream_insertion_operator)
{
	auto str = [](ErrorCode code) -> std::string
	{
		std::ostringstream oss;
		oss << code;
		return oss.str();
	};

	// Each named value writes its identifier with no surrounding whitespace,
	// punctuation, or namespace qualification.
	EXPECT_EQ(str(ErrorCode::None), "None");
	EXPECT_EQ(str(ErrorCode::SdlInitFailed), "SdlInitFailed");
	EXPECT_EQ(str(ErrorCode::WindowCreateFailed), "WindowCreateFailed");
	EXPECT_EQ(str(ErrorCode::SurfaceCreateFailed), "SurfaceCreateFailed");
	EXPECT_EQ(str(ErrorCode::AlreadyInitialized), "AlreadyInitialized");
	EXPECT_EQ(str(ErrorCode::NotInitialized), "NotInitialized");

	// An out-of-range value writes "ErrorCode(N)" where N is the decimal
	// representation of the underlying integer.
	const auto outOfRange = static_cast<ErrorCode>(99u);
	EXPECT_EQ(str(outOfRange), "ErrorCode(99)");

	// The operator returns the same ostream reference (supports chaining).
	std::ostringstream oss;
	std::ostream& returned = (oss << ErrorCode::None);
	EXPECT_EQ(&returned, &oss);

	// Chaining produces concatenated output.
	std::ostringstream chained;
	chained << ErrorCode::None << "-" << ErrorCode::SdlInitFailed;
	EXPECT_EQ(chained.str(), "None-SdlInitFailed");
}
