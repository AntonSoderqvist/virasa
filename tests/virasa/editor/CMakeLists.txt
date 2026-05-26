add_executable(virasa_editor_tests
    test_Buffer.cpp
    test_Motions.cpp
    test_CommandBarView.cpp
    test_EditorView.cpp
    test_HierarchyView.cpp
    test_EntityEditorView.cpp
    test_ViewManager.cpp
    test_EditorApp.cpp
)

target_link_libraries(virasa_editor_tests
    PRIVATE
        virasa_editor_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(virasa_editor_tests)
