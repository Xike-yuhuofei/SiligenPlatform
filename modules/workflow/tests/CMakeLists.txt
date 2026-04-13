cmake_minimum_required(VERSION 3.20)

set(WORKFLOW_MODULE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
set(WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS
    "${WORKFLOW_MODULE_DIR}"
    "${WORKFLOW_MODULE_DIR}/application/include"
    "${WORKFLOW_MODULE_DIR}/domain/include"
)

set(WORKFLOW_REQUIRED_TEST_ROOTS
    "${CMAKE_CURRENT_SOURCE_DIR}/contract/CMakeLists.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/CMakeLists.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/canonical/CMakeLists.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/integration/CMakeLists.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/regression/CMakeLists.txt"
)

foreach(WORKFLOW_REQUIRED_TEST_ROOT IN LISTS WORKFLOW_REQUIRED_TEST_ROOTS)
    if(NOT EXISTS "${WORKFLOW_REQUIRED_TEST_ROOT}")
        message(FATAL_ERROR "workflow canonical tests root missing: ${WORKFLOW_REQUIRED_TEST_ROOT}")
    endif()
endforeach()

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/contract"
    "${CMAKE_BINARY_DIR}/tests/workflow-contract"
)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/unit"
    "${CMAKE_BINARY_DIR}/tests/workflow-unit"
)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/canonical"
    "${CMAKE_BINARY_DIR}/tests/workflow-canonical"
)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/integration"
    "${CMAKE_BINARY_DIR}/tests/workflow-integration"
)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/regression"
    "${CMAKE_BINARY_DIR}/tests/workflow-regression"
)
