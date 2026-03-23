cmake_minimum_required(VERSION 3.20)

set(SHARED_KERNEL_TESTS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../packages/shared-kernel/tests"
)
if(NOT EXISTS "${SHARED_KERNEL_TESTS_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "未找到 shared-kernel canonical 测试入口: ${SHARED_KERNEL_TESTS_DIR}")
endif()

add_subdirectory(
    "${SHARED_KERNEL_TESTS_DIR}"
    "${CMAKE_BINARY_DIR}/packages/shared-kernel-tests"
)

set(PROCESS_RUNTIME_CORE_TESTS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../packages/process-runtime-core/tests"
)
if(NOT EXISTS "${PROCESS_RUNTIME_CORE_TESTS_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "未找到 process-runtime-core canonical 测试入口: ${PROCESS_RUNTIME_CORE_TESTS_DIR}")
endif()

add_subdirectory(
    "${PROCESS_RUNTIME_CORE_TESTS_DIR}"
    "${CMAKE_BINARY_DIR}/packages/process-runtime-core-tests"
)

set(RUNTIME_HOST_TESTS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../packages/runtime-host/tests"
)
if(EXISTS "${RUNTIME_HOST_TESTS_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_TESTS_DIR}"
        "${CMAKE_BINARY_DIR}/packages/runtime-host-tests"
    )
endif()
