cmake_minimum_required(VERSION 3.20)

set(RUNTIME_EXECUTION_REQUIRED_TEST_ROOTS
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/CMakeLists.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime-host/CMakeLists.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/regression/CMakeLists.txt"
)

foreach(RUNTIME_EXECUTION_REQUIRED_TEST_ROOT IN LISTS RUNTIME_EXECUTION_REQUIRED_TEST_ROOTS)
    if(NOT EXISTS "${RUNTIME_EXECUTION_REQUIRED_TEST_ROOT}")
        message(FATAL_ERROR "runtime-execution canonical tests root missing: ${RUNTIME_EXECUTION_REQUIRED_TEST_ROOT}")
    endif()
endforeach()

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/unit"
    "${CMAKE_BINARY_DIR}/tests/runtime-execution-unit"
)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime-host"
    "${CMAKE_BINARY_DIR}/tests/runtime-execution-runtime-host"
)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/regression"
    "${CMAKE_BINARY_DIR}/tests/runtime-execution-regression"
)
