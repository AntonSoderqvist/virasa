add_library(virasa_sim_lib STATIC
    Tick.cpp
)

target_include_directories(virasa_sim_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
