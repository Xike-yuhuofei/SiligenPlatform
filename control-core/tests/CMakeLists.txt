# tests/CMakeLists.txt

set(PROCESS_RUNTIME_CORE_TESTS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../packages/process-runtime-core/tests"
)

if(NOT EXISTS "${PROCESS_RUNTIME_CORE_TESTS_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "未找到 process-runtime-core canonical 测试入口: ${PROCESS_RUNTIME_CORE_TESTS_DIR}")
endif()

add_subdirectory(
    "${PROCESS_RUNTIME_CORE_TESTS_DIR}"
    "${CMAKE_BINARY_DIR}/packages/process-runtime-core-tests"
)
