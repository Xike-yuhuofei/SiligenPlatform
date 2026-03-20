cmake_minimum_required(VERSION 3.20)

message(STATUS "=== Apps 入口 ===")

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/control-runtime"
    "${CMAKE_BINARY_DIR}/apps/control-runtime"
)
add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/control-tcp-server"
    "${CMAKE_BINARY_DIR}/apps/control-tcp-server"
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/control-cli/main.cpp")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/control-cli"
        "${CMAKE_BINARY_DIR}/apps/control-cli"
    )
else()
    message(WARNING "跳过 workspace apps/control-cli：main.cpp 缺失")
endif()
