cmake_minimum_required(VERSION 3.20)

message(STATUS "=== Apps 入口 ===")

set(SILIGEN_WORKSPACE_APPS_DIR "${CMAKE_SOURCE_DIR}/../apps")

add_subdirectory(
    "${SILIGEN_WORKSPACE_APPS_DIR}/control-runtime"
    "${CMAKE_BINARY_DIR}/apps/control-runtime"
)
add_subdirectory(
    "${SILIGEN_WORKSPACE_APPS_DIR}/control-tcp-server"
    "${CMAKE_BINARY_DIR}/apps/control-tcp-server"
)
if(EXISTS "${SILIGEN_WORKSPACE_APPS_DIR}/control-cli/main.cpp")
    add_subdirectory(
        "${SILIGEN_WORKSPACE_APPS_DIR}/control-cli"
        "${CMAKE_BINARY_DIR}/apps/control-cli"
    )
else()
    message(WARNING "跳过 workspace apps/control-cli：main.cpp 缺失")
endif()
