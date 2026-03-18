cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_shared_kernel AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../shared-kernel/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../shared-kernel"
        "${CMAKE_BINARY_DIR}/packages/shared-kernel"
    )
endif()

add_library(siligen_device_contracts INTERFACE)

target_include_directories(siligen_device_contracts INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(siligen_device_contracts INTERFACE
    siligen_shared_kernel
)

target_compile_features(siligen_device_contracts INTERFACE cxx_std_17)
