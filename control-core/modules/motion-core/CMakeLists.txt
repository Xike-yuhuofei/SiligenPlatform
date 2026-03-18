cmake_minimum_required(VERSION 3.20)

add_library(siligen_motion_core STATIC
    src/safety/services/interlock_policy.cpp
)

target_include_directories(siligen_motion_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(siligen_motion_core PUBLIC
    siligen_shared_kernel
)

target_compile_features(siligen_motion_core PUBLIC cxx_std_17)
