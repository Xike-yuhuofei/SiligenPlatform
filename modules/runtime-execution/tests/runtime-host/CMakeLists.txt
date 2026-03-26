cmake_minimum_required(VERSION 3.20)

set(SILIGEN_RUNTIME_HOST_TESTS_CANONICAL_DIR
    "${SILIGEN_WORKSPACE_ROOT}/modules/runtime-execution/runtime/host/tests"
)

if(NOT TARGET siligen_runtime_host_unit_tests)
    if(NOT EXISTS "${SILIGEN_RUNTIME_HOST_TESTS_CANONICAL_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "runtime-execution module tests/runtime-host entry requires canonical runtime/host/tests/CMakeLists.txt"
        )
    endif()

    add_subdirectory(
        "${SILIGEN_RUNTIME_HOST_TESTS_CANONICAL_DIR}"
        "${CMAKE_BINARY_DIR}/rtx-host-unit"
    )
endif()
