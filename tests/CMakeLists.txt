cmake_minimum_required(VERSION 3.20)

get_filename_component(SILIGEN_TESTS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(SILIGEN_WORKSPACE_ROOT_CANDIDATE "${SILIGEN_TESTS_SOURCE_DIR}/.." ABSOLUTE)
set(SILIGEN_LAYOUT_FILE "${SILIGEN_WORKSPACE_ROOT_CANDIDATE}/cmake/workspace-layout.env")
include("${SILIGEN_WORKSPACE_ROOT_CANDIDATE}/cmake/LoadWorkspaceLayout.cmake")
siligen_load_workspace_layout(
    "${SILIGEN_WORKSPACE_ROOT_CANDIDATE}"
    "${SILIGEN_LAYOUT_FILE}"
)
set(SILIGEN_WORKSPACE_ROOT "${SILIGEN_WORKSPACE_ROOT_CANDIDATE}")
if(NOT "${SILIGEN_WORKSPACE_ROOT}" STREQUAL "${SILIGEN_WORKSPACE_ROOT_CANDIDATE}")
    message(FATAL_ERROR
        "tests workspace root mismatch: "
        "resolved='${SILIGEN_WORKSPACE_ROOT}', candidate='${SILIGEN_WORKSPACE_ROOT_CANDIDATE}'"
    )
endif()
if(NOT "${SILIGEN_WORKSPACE_ROOT}" STREQUAL "${CMAKE_SOURCE_DIR}")
    message(FATAL_ERROR
        "tests root must resolve to canonical superbuild source root: CMAKE_SOURCE_DIR='${CMAKE_SOURCE_DIR}', "
        "resolved='${SILIGEN_WORKSPACE_ROOT}'"
    )
endif()

set(SHARED_KERNEL_TESTS_DIR
    "${SILIGEN_SHARED_KERNEL_DIR}/tests"
)
if(NOT EXISTS "${SHARED_KERNEL_TESTS_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "未找到 shared-kernel canonical 测试入口: ${SHARED_KERNEL_TESTS_DIR}")
endif()

add_subdirectory(
    "${SHARED_KERNEL_TESTS_DIR}"
    "${CMAKE_BINARY_DIR}/packages/shared-kernel-tests"
)

set(PROCESS_RUNTIME_CORE_TESTS_DIR
    "${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/tests"
)
if(NOT EXISTS "${PROCESS_RUNTIME_CORE_TESTS_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "未找到 process-runtime-core canonical 测试入口: ${PROCESS_RUNTIME_CORE_TESTS_DIR}")
endif()

add_subdirectory(
    "${PROCESS_RUNTIME_CORE_TESTS_DIR}"
    "${CMAKE_BINARY_DIR}/packages/process-runtime-core-tests"
)

set(RUNTIME_HOST_TESTS_DIR
    "${SILIGEN_RUNTIME_HOST_DIR}/tests"
)
if(EXISTS "${RUNTIME_HOST_TESTS_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_TESTS_DIR}"
        "${CMAKE_BINARY_DIR}/packages/runtime-host-tests"
    )
endif()
