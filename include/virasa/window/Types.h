// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#ifndef VIRASA_WINDOW_TYPES_H
#define VIRASA_WINDOW_TYPES_H

#include <cstdint>
#include <ostream>

namespace virasa
{

/**
 * @brief Holds integer pixel dimensions for a 2D size.
 *
 * Size2D stores a width and height in pixels. It is copyable, movable,
 * and default-constructible. A default-constructed Size2D has both
 * width and height equal to 0.
 */
struct Size2D
{
	public:
	/// @brief Width in pixels.
	uint32_t width = 0;

	/// @brief Height in pixels.
	uint32_t height = 0;
};

/**
 * @brief Error codes returned by fallible operations.
 *
 * ErrorCode is a closed enum class with underlying type uint8_t.
 * None (0) represents success; all other values represent distinct
 * failure modes.
 */
enum class ErrorCode : uint8_t
{
	None = 0,		   ///< Operation completed successfully.
	SdlInitFailed,	   ///< SDL_Init() failed.
	WindowCreateFailed,  ///< SDL_CreateWindow() failed.
	SurfaceCreateFailed, ///< SDL_Vulkan_CreateSurface() failed.
	AlreadyInitialized,  ///< Initialize() called on an already-initialized Platform.
	NotInitialized,	   ///< Operation attempted on an uninitialized Platform.
};

/**
 * @brief Writes an ErrorCode to a std::ostream.
 *
 * Writes the identifier of the ErrorCode value as a plain string with
 * no surrounding whitespace, punctuation, or namespace qualification.
 * If the value does not correspond to any declared ErrorCode, writes
 * "ErrorCode(N)" where N is the decimal representation of the
 * underlying integer value.
 *
 * @param os   The output stream to write to.
 * @param code The ErrorCode value to write.
 * @return The same ostream reference, to support chaining.
 */
inline std::ostream& operator<<(std::ostream& os, ErrorCode code)
{
	switch (code)
	{
		case ErrorCode::None:
			return os << "None";
		case ErrorCode::SdlInitFailed:
			return os << "SdlInitFailed";
		case ErrorCode::WindowCreateFailed:
			return os << "WindowCreateFailed";
		case ErrorCode::SurfaceCreateFailed:
			return os << "SurfaceCreateFailed";
		case ErrorCode::AlreadyInitialized:
			return os << "AlreadyInitialized";
		case ErrorCode::NotInitialized:
			return os << "NotInitialized";
		default:
			return os << "ErrorCode(" << static_cast<unsigned>(static_cast<uint8_t>(code))
				    << ")";
	}
}

} // namespace virasa

#endif // VIRASA_WINDOW_TYPES_H
