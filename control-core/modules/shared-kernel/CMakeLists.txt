cmake_minimum_required(VERSION 3.20)

add_library(siligen_shared_kernel STATIC
    src/strings/string_manipulator.cpp
)

target_include_directories(siligen_shared_kernel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

target_compile_features(siligen_shared_kernel PUBLIC cxx_std_17)
