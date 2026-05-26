#ifndef VIRASA_EDITOR_EDITORAPP_H
#define VIRASA_EDITOR_EDITORAPP_H

#include "virasa/math/Types.h"
#include <cstdint>

namespace virasa::editor
{

/**
 * @brief Top-level orchestrator for the Virasa editor process.
 *
 * EditorApp is the single entry point instantiated by main.cpp. It owns the
 * camera-orbit input state across loop iterations and drives the full
 * application lifecycle through Run().
 */
class EditorApp final
{
public:
	EditorApp() = default;

	EditorApp(const EditorApp&) = delete;
	EditorApp& operator=(const EditorApp&) = delete;
	EditorApp(EditorApp&&) = delete;
	EditorApp& operator=(EditorApp&&) = delete;

	/**
	 * @brief Execute the complete editor lifecycle.
	 * @param argc Reserved for future command-line argument parsing (unused).
	 * @param argv Reserved for future command-line argument parsing (unused).
	 * @return 0 on clean shutdown, -1 on any subsystem initialization failure.
	 */
	int Run(int argc, char** argv);

private:
	float _cameraYaw   = 0.0f;
	float _cameraPitch = 0.0f;
	virasa::math::Vec3 _cameraPosition = {4.0f, 4.0f, 3.0f};
};

} // namespace virasa::editor

#endif // VIRASA_EDITOR_EDITORAPP_H
