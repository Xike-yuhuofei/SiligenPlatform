cmake_minimum_required(VERSION 3.20)

set(SILIGEN_RUNTIME_EXECUTION_INTEGRATION_CANONICAL_DIR
    "${SILIGEN_WORKSPACE_ROOT}/modules/runtime-execution/runtime/host/tests/integration"
)

if(NOT TARGET runtime_execution_integration_host_bootstrap_smoke)
    if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_INTEGRATION_CANONICAL_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "runtime-execution integration entry requires canonical runtime/host/tests/integration/CMakeLists.txt"
        )
    endif()

    add_subdirectory(
        "${SILIGEN_RUNTIME_EXECUTION_INTEGRATION_CANONICAL_DIR}"
        "${CMAKE_BINARY_DIR}/rtx-host-int"
    )
endif()
