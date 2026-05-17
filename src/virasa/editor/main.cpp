#include <quill/LogMacros.h>
#include <quill/Logger.h>

#include "virasa/core/Logger.h"
#include "virasa/window/Events.h"
#include "virasa/window/Platform.h"
#include "virasa/window/Types.h"

int main(int argc, char** argv)
{
	quill::Logger* logger = virasa::Logger::GetLogger("Main");
	LOG_INFO(logger, "Entry point reached.");

	virasa::window::Platform platform;
	virasa::ErrorCode initResult = platform.Initialize();
	if (initResult != virasa::ErrorCode::None)
	{
		LOG_ERROR(logger,
			"Platform initialization failed with error code: {}",
			static_cast<int>(initResult));
		return -1;
	}

	while (true)
	{
		std::span<const virasa::Event> events = platform.PollEvents();
		for (const virasa::Event& event : events)
		{
			if (event.type == virasa::EventType::KeyDown &&
				event.keyboard.key == virasa::KeyCode::Escape)
			{
				LOG_INFO(logger, "Escape pressed, exiting.");
				return 0;
			}
		}
	}

	return 0;
}
