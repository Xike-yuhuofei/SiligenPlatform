cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_application_motion)
    message(FATAL_ERROR "workflow regression tests 需要先加载 siligen_application_motion target")
endif()

set(WORKFLOW_REGRESSION_PATH_EXECUTION_TEST_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/../process-runtime-core/unit/motion/DeterministicPathExecutionUseCaseTest.cpp"
)

if(EXISTS "${WORKFLOW_REGRESSION_PATH_EXECUTION_TEST_SOURCE}")
    add_executable(workflow_regression_deterministic_path_execution_smoke
        "${WORKFLOW_REGRESSION_PATH_EXECUTION_TEST_SOURCE}"
    )

    target_include_directories(workflow_regression_deterministic_path_execution_smoke PRIVATE
        ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
    )
    target_link_libraries(workflow_regression_deterministic_path_execution_smoke PRIVATE
        siligen_application_motion
        siligen_types
        siligen_utils
    )
    target_compile_features(workflow_regression_deterministic_path_execution_smoke PRIVATE cxx_std_17)

    process_runtime_core_add_test_with_runtime_environment(
        workflow_regression_deterministic_path_execution_smoke
        workflow_regression_deterministic_path_execution_smoke
    )
endif()

if(NOT TARGET workflow_regression_tests)
    add_custom_target(workflow_regression_tests)
endif()

if(TARGET workflow_regression_deterministic_path_execution_smoke)
    add_dependencies(workflow_regression_tests
        workflow_regression_deterministic_path_execution_smoke
    )
endif()
