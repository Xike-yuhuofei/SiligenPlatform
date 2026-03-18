cmake_minimum_required(VERSION 3.20)

set(SILIGEN_SHARED_KERNEL_THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")
if(NOT EXISTS "${SILIGEN_SHARED_KERNEL_THIRD_PARTY_DIR}")
    set(SILIGEN_SHARED_KERNEL_THIRD_PARTY_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/../../control-core/third_party"
    )
endif()

if(NOT EXISTS "${SILIGEN_SHARED_KERNEL_THIRD_PARTY_DIR}")
    message(FATAL_ERROR "shared-kernel requires a reachable third_party directory for Boost headers.")
endif()

add_library(siligen_shared_kernel STATIC
    src/strings/string_manipulator.cpp
)

target_include_directories(siligen_shared_kernel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SILIGEN_SHARED_KERNEL_THIRD_PARTY_DIR}
)

target_compile_features(siligen_shared_kernel PUBLIC cxx_std_17)
