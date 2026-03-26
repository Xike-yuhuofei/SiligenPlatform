cmake_minimum_required(VERSION 3.20)

set(SILIGEN_RUNTIME_EXECUTION_REGRESSION_CANONICAL_DIR
    "${SILIGEN_WORKSPACE_ROOT}/modules/runtime-execution/runtime/host/tests/regression"
)

if(NOT TARGET runtime_execution_regression_tests)
    if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_REGRESSION_CANONICAL_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "runtime-execution regression entry requires canonical runtime/host/tests/regression/CMakeLists.txt"
        )
    endif()

    add_subdirectory(
        "${SILIGEN_RUNTIME_EXECUTION_REGRESSION_CANONICAL_DIR}"
        "${CMAKE_BINARY_DIR}/rtx-host-reg"
    )
endif()
