cmake_minimum_required(VERSION 3.20)

add_library(siligen_device_hal INTERFACE)

target_include_directories(siligen_device_hal INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(siligen_device_hal INTERFACE
    siligen_device_adapters
    siligen_device_contracts
)

target_compile_features(siligen_device_hal INTERFACE cxx_std_17)
