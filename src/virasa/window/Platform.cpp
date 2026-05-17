// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#include "virasa/window/Platform.h"

#include <cassert>
#include <cstring>

namespace virasa::window
{

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

Platform::~Platform()
{
	Shutdown();
}

// ---------------------------------------------------------------------------
// Initialize / Shutdown
// ---------------------------------------------------------------------------

ErrorCode Platform::Initialize(const char* window_title, uint32_t pixel_width, uint32_t pixel_height)
{
	if (_initialized)
	{
		return ErrorCode::AlreadyInitialized;
	}

	if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
	{
		return ErrorCode::SdlInitFailed;
	}

	_window = SDL_CreateWindow(window_title, static_cast<int>(pixel_width), static_cast<int>(pixel_height),
		SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	if (!_window)
	{
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return ErrorCode::WindowCreateFailed;
	}

	_windowSize.width = pixel_width;
	_windowSize.height = pixel_height;
	_initialized = true;

	// Failure to enable text input does not cause Initialize to fail.
	StartTextInput();

	return ErrorCode::None;
}

void Platform::Shutdown()
{
	if (!_initialized)
	{
		return;
	}

	StopTextInput();

	if (_window)
	{
		SDL_DestroyWindow(_window);
		_window = nullptr;
	}

	SDL_QuitSubSystem(SDL_INIT_VIDEO);

	_surface = VK_NULL_HANDLE;
	_windowSize = Size2D{};
	_minimized = false;
	_quitRequested = false;
	_events.clear();
	_rawEvents.clear();
	_initialized = false;
}

// ---------------------------------------------------------------------------
// CreateSurface
// ---------------------------------------------------------------------------

ErrorCode Platform::CreateSurface(VkInstance instance, VkSurfaceKHR* out_surface)
{
	assert(out_surface != nullptr);

	if (!_initialized)
	{
		return ErrorCode::NotInitialized;
	}

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (!SDL_Vulkan_CreateSurface(_window, instance, nullptr, &surface))
	{
		return ErrorCode::SurfaceCreateFailed;
	}

	_surface = surface;
	*out_surface = surface;
	return ErrorCode::None;
}

// ---------------------------------------------------------------------------
// GetRequiredVulkanExtensions
// ---------------------------------------------------------------------------

const char* const* Platform::GetRequiredVulkanExtensions(uint32_t* out_count)
{
	return SDL_Vulkan_GetInstanceExtensions(out_count);
}

// ---------------------------------------------------------------------------
// PollEvents
// ---------------------------------------------------------------------------

KeyCode Platform::TranslateSdlKeycode(SDL_Keycode sdl_key) const noexcept
{
	switch (sdl_key)
	{
		case SDLK_A:
			return KeyCode::A;
		case SDLK_B:
			return KeyCode::B;
		case SDLK_C:
			return KeyCode::C;
		case SDLK_D:
			return KeyCode::D;
		case SDLK_E:
			return KeyCode::E;
		case SDLK_F:
			return KeyCode::F;
		case SDLK_G:
			return KeyCode::G;
		case SDLK_H:
			return KeyCode::H;
		case SDLK_I:
			return KeyCode::I;
		case SDLK_J:
			return KeyCode::J;
		case SDLK_K:
			return KeyCode::K;
		case SDLK_L:
			return KeyCode::L;
		case SDLK_M:
			return KeyCode::M;
		case SDLK_N:
			return KeyCode::N;
		case SDLK_O:
			return KeyCode::O;
		case SDLK_P:
			return KeyCode::P;
		case SDLK_Q:
			return KeyCode::Q;
		case SDLK_R:
			return KeyCode::R;
		case SDLK_S:
			return KeyCode::S;
		case SDLK_T:
			return KeyCode::T;
		case SDLK_U:
			return KeyCode::U;
		case SDLK_V:
			return KeyCode::V;
		case SDLK_W:
			return KeyCode::W;
		case SDLK_X:
			return KeyCode::X;
		case SDLK_Y:
			return KeyCode::Y;
		case SDLK_Z:
			return KeyCode::Z;
		case SDLK_0:
			return KeyCode::Num0;
		case SDLK_1:
			return KeyCode::Num1;
		case SDLK_2:
			return KeyCode::Num2;
		case SDLK_3:
			return KeyCode::Num3;
		case SDLK_4:
			return KeyCode::Num4;
		case SDLK_5:
			return KeyCode::Num5;
		case SDLK_6:
			return KeyCode::Num6;
		case SDLK_7:
			return KeyCode::Num7;
		case SDLK_8:
			return KeyCode::Num8;
		case SDLK_9:
			return KeyCode::Num9;
		case SDLK_ESCAPE:
			return KeyCode::Escape;
		case SDLK_RETURN:
			return KeyCode::Enter;
		case SDLK_TAB:
			return KeyCode::Tab;
		case SDLK_SPACE:
			return KeyCode::Space;
		case SDLK_BACKSPACE:
			return KeyCode::Backspace;
		case SDLK_LEFT:
			return KeyCode::Left;
		case SDLK_RIGHT:
			return KeyCode::Right;
		case SDLK_UP:
			return KeyCode::Up;
		case SDLK_DOWN:
			return KeyCode::Down;
		case SDLK_LSHIFT:
			return KeyCode::LShift;
		case SDLK_RSHIFT:
			return KeyCode::RShift;
		case SDLK_LCTRL:
			return KeyCode::LCtrl;
		case SDLK_RCTRL:
			return KeyCode::RCtrl;
		case SDLK_LALT:
			return KeyCode::LAlt;
		case SDLK_RALT:
			return KeyCode::RAlt;
		case SDLK_F1:
			return KeyCode::F1;
		case SDLK_F2:
			return KeyCode::F2;
		case SDLK_F3:
			return KeyCode::F3;
		case SDLK_F4:
			return KeyCode::F4;
		case SDLK_F5:
			return KeyCode::F5;
		case SDLK_F6:
			return KeyCode::F6;
		case SDLK_F7:
			return KeyCode::F7;
		case SDLK_F8:
			return KeyCode::F8;
		case SDLK_F9:
			return KeyCode::F9;
		case SDLK_F10:
			return KeyCode::F10;
		case SDLK_F11:
			return KeyCode::F11;
		case SDLK_F12:
			return KeyCode::F12;
		default:
			return KeyCode::Unknown;
	}
}

MouseButton Platform::TranslateSdlMouseButton(uint8_t sdl_button) const noexcept
{
	switch (sdl_button)
	{
		case SDL_BUTTON_LEFT:
			return MouseButton::Left;
		case SDL_BUTTON_MIDDLE:
			return MouseButton::Middle;
		case SDL_BUTTON_RIGHT:
			return MouseButton::Right;
		case SDL_BUTTON_X1:
			return MouseButton::X1;
		case SDL_BUTTON_X2:
			return MouseButton::X2;
		default:
			return MouseButton::Left;
	}
}

std::span<const Event> Platform::PollEvents()
{
	_events.clear();
	_rawEvents.clear();

	if (!_initialized)
	{
		return {};
	}

	SDL_Event sdlEvent;
	while (SDL_PollEvent(&sdlEvent))
	{
		_rawEvents.push_back(sdlEvent);

		Event ev{};
		ev.timestamp = sdlEvent.common.timestamp;
		bool translated = true;

		switch (sdlEvent.type)
		{
			case SDL_EVENT_QUIT:
			{
				ev.type = EventType::Quit;
				_quitRequested = true;
				break;
			}

			case SDL_EVENT_WINDOW_RESIZED:
			{
				ev.type = EventType::WindowResized;
				ev.resize.width = static_cast<uint32_t>(sdlEvent.window.data1);
				ev.resize.height = static_cast<uint32_t>(sdlEvent.window.data2);
				_windowSize.width = ev.resize.width;
				_windowSize.height = ev.resize.height;
				break;
			}

			case SDL_EVENT_WINDOW_MINIMIZED:
			{
				ev.type = EventType::WindowMinimized;
				_minimized = true;
				break;
			}

			case SDL_EVENT_WINDOW_RESTORED:
			{
				ev.type = EventType::WindowRestored;
				_minimized = false;
				break;
			}

			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			{
				ev.type = EventType::WindowFocusGained;
				break;
			}

			case SDL_EVENT_WINDOW_FOCUS_LOST:
			{
				ev.type = EventType::WindowFocusLost;
				break;
			}

			case SDL_EVENT_KEY_DOWN:
			{
				ev.type = EventType::KeyDown;
				ev.keyboard.key = TranslateSdlKeycode(sdlEvent.key.key);
				ev.keyboard.repeat = sdlEvent.key.repeat;
				break;
			}

			case SDL_EVENT_KEY_UP:
			{
				ev.type = EventType::KeyUp;
				ev.keyboard.key = TranslateSdlKeycode(sdlEvent.key.key);
				ev.keyboard.repeat = false;
				break;
			}

			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			{
				ev.type = EventType::MouseButtonDown;
				ev.mouseButton.button = TranslateSdlMouseButton(sdlEvent.button.button);
				ev.mouseButton.x = static_cast<int32_t>(sdlEvent.button.x);
				ev.mouseButton.y = static_cast<int32_t>(sdlEvent.button.y);
				break;
			}

			case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				ev.type = EventType::MouseButtonUp;
				ev.mouseButton.button = TranslateSdlMouseButton(sdlEvent.button.button);
				ev.mouseButton.x = static_cast<int32_t>(sdlEvent.button.x);
				ev.mouseButton.y = static_cast<int32_t>(sdlEvent.button.y);
				break;
			}

			case SDL_EVENT_MOUSE_MOTION:
			{
				ev.type = EventType::MouseMoved;
				ev.mouseMove.x = static_cast<int32_t>(sdlEvent.motion.x);
				ev.mouseMove.y = static_cast<int32_t>(sdlEvent.motion.y);
				ev.mouseMove.deltaX = static_cast<int32_t>(sdlEvent.motion.xrel);
				ev.mouseMove.deltaY = static_cast<int32_t>(sdlEvent.motion.yrel);
				break;
			}

			case SDL_EVENT_MOUSE_WHEEL:
			{
				ev.type = EventType::MouseWheel;
				ev.mouseWheel.scrollX = sdlEvent.wheel.x;
				ev.mouseWheel.scrollY = sdlEvent.wheel.y;
				break;
			}

			case SDL_EVENT_TEXT_INPUT:
			{
				ev.type = EventType::TextInput;
				const char* src = sdlEvent.text.text;
				size_t srcLen = std::strlen(src);
				constexpr size_t kMaxPayload = 31;
				if (srcLen > kMaxPayload)
				{
					// Truncation: log warning (implementation detail, not pinned).
					srcLen = kMaxPayload;
				}
				std::memcpy(ev.textInput.utf8, src, srcLen);
				ev.textInput.utf8[srcLen] = '\0';
				ev.textInput.length = static_cast<uint8_t>(srcLen);
				break;
			}

			default:
			{
				translated = false;
				break;
			}
		}

		if (translated)
		{
			_events.push_back(ev);
		}
	}

	return std::span<const Event>(_events);
}

// ---------------------------------------------------------------------------
// Observers
// ---------------------------------------------------------------------------

Size2D Platform::GetWindowSize() const
{
	return _windowSize;
}

bool Platform::IsMinimized() const
{
	return _minimized;
}

bool Platform::IsQuitRequested() const
{
	return _quitRequested;
}

SDL_Window* Platform::GetWindowHandle() const
{
	return _window;
}

std::span<const SDL_Event> Platform::GetRawEvents() const
{
	return std::span<const SDL_Event>(_rawEvents);
}

VkSurfaceKHR Platform::GetSurface() const
{
	return _surface;
}

// ---------------------------------------------------------------------------
// Text input
// ---------------------------------------------------------------------------

ErrorCode Platform::StartTextInput()
{
	if (!_initialized)
	{
		return ErrorCode::NotInitialized;
	}

	if (!SDL_StartTextInput(_window))
	{
		return ErrorCode::SdlInitFailed;
	}

	return ErrorCode::None;
}

void Platform::StopTextInput()
{
	if (!_initialized || !_window)
	{
		return;
	}

	SDL_StopTextInput(_window);
}

} // namespace virasa::window
