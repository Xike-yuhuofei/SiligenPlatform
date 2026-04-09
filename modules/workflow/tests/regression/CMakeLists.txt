cmake_minimum_required(VERSION 3.20)

set(WORKFLOW_REGRESSION_TEST_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/DispensingWorkflowModeResolutionTest.cpp"
)

if(EXISTS "${WORKFLOW_REGRESSION_TEST_SOURCE}")
    add_executable(workflow_regression_dispensing_mode_resolution_smoke
        "${WORKFLOW_REGRESSION_TEST_SOURCE}"
    )

    target_include_directories(workflow_regression_dispensing_mode_resolution_smoke PRIVATE
        ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
    )
    if(TARGET GTest::gtest_main)
        target_link_libraries(workflow_regression_dispensing_mode_resolution_smoke PRIVATE GTest::gtest_main)
    elseif(TARGET gtest_main)
        target_link_libraries(workflow_regression_dispensing_mode_resolution_smoke PRIVATE gtest_main)
    else()
        message(FATAL_ERROR "GoogleTest target not found")
    endif()
    target_compile_features(workflow_regression_dispensing_mode_resolution_smoke PRIVATE cxx_std_17)

    siligen_add_test_with_runtime_environment(
        workflow_regression_dispensing_mode_resolution_smoke
        workflow_regression_dispensing_mode_resolution_smoke
    )
endif()

if(NOT TARGET workflow_regression_tests)
    add_custom_target(workflow_regression_tests)
endif()

if(TARGET workflow_regression_dispensing_mode_resolution_smoke)
    add_dependencies(workflow_regression_tests
        workflow_regression_dispensing_mode_resolution_smoke
    )
endif()
