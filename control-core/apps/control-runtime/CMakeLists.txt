cmake_minimum_required(VERSION 3.20)

add_library(siligen_control_runtime INTERFACE)
add_library(siligen_control_runtime_app ALIAS siligen_control_runtime)

target_link_libraries(siligen_control_runtime INTERFACE
    siligen_runtime_host
)
