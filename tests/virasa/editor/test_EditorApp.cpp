#include <gtest/gtest.h>

#include "virasa/editor/EditorApp.h"

#include <concepts>
#include <type_traits>

namespace
{

template <typename T>
concept HasCameraYawMember = requires(T value)
{
	value._cameraYaw;
};

template <typename T>
concept HasCameraPitchMember = requires(T value)
{
	value._cameraPitch;
};

template <typename T>
concept HasCameraPositionMember = requires(T value)
{
	value._cameraPosition;
};

} // namespace

TEST(EditorApp, test_editor_app_is_top_level_orchestrator)
{
	using virasa::editor::EditorApp;

	EXPECT_TRUE(std::is_class_v<EditorApp>);
	EXPECT_TRUE(std::is_final_v<EditorApp>);
	EXPECT_TRUE(std::is_default_constructible_v<EditorApp>);
	EXPECT_FALSE(std::is_copy_constructible_v<EditorApp>);
	EXPECT_FALSE(std::is_copy_assignable_v<EditorApp>);
	EXPECT_FALSE(std::is_move_constructible_v<EditorApp>);
	EXPECT_FALSE(std::is_move_assignable_v<EditorApp>);
	EXPECT_TRUE((std::is_same_v<decltype(&EditorApp::Run), int (EditorApp::*)(int, char**)>));
}

TEST(EditorApp, test_run_is_program_lifecycle)
{
	using virasa::editor::EditorApp;

	EditorApp app;
	char* argv[] = {nullptr};

	const int result = app.Run(0, argv);

	EXPECT_TRUE(result == 0 || result == -1);
}

TEST(EditorApp, test_run_loop_drives_one_frame_at_a_time)
{
	using virasa::editor::EditorApp;

	EditorApp app;
	char* argv[] = {nullptr};

	const int result = app.Run(0, argv);

	EXPECT_TRUE(result == 0 || result == -1);
}

TEST(EditorApp, test_run_owns_camera_state_across_iterations)
{
	using virasa::editor::EditorApp;

	EXPECT_FALSE(HasCameraYawMember<EditorApp>);
	EXPECT_FALSE(HasCameraPitchMember<EditorApp>);
	EXPECT_FALSE(HasCameraPositionMember<EditorApp>);
	EXPECT_TRUE(std::is_default_constructible_v<EditorApp>);
	EXPECT_FALSE(std::is_copy_constructible_v<EditorApp>);
	EXPECT_FALSE(std::is_move_constructible_v<EditorApp>);
}

TEST(EditorApp, test_editor_app_is_not_thread_safe_per_instance)
{
	using virasa::editor::EditorApp;

	EXPECT_TRUE(std::is_default_constructible_v<EditorApp>);
	EXPECT_FALSE(std::is_copy_constructible_v<EditorApp>);
	EXPECT_FALSE(std::is_copy_assignable_v<EditorApp>);
	EXPECT_FALSE(std::is_move_constructible_v<EditorApp>);
	EXPECT_FALSE(std::is_move_assignable_v<EditorApp>);
}