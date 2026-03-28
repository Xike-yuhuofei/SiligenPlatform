cmake_minimum_required(VERSION 3.20)

message(STATUS "=== Apps 入口 ===")

foreach(target_app IN ITEMS runtime-service planner-cli runtime-gateway)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${target_app}/CMakeLists.txt")
        add_subdirectory(
            "${CMAKE_CURRENT_SOURCE_DIR}/${target_app}"
            "${CMAKE_BINARY_DIR}/apps/${target_app}"
        )
    endif()
endforeach()
