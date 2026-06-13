add_executable(virasa_sim_tests
    test_Tick.cpp
)

target_link_libraries(virasa_sim_tests
    PRIVATE
        virasa_sim_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(virasa_sim_tests)
