cmake_minimum_required(VERSION 3.20)

set(WORKFLOW_REGRESSION_FORBIDDEN_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/DispensingWorkflowModeResolutionTest.cpp"
)
foreach(forbidden_source IN LISTS WORKFLOW_REGRESSION_FORBIDDEN_SOURCES)
    if(EXISTS "${forbidden_source}")
        message(FATAL_ERROR "workflow regression tests must remain M0-only registration shells: ${forbidden_source}")
    endif()
endforeach()

if(NOT TARGET workflow_regression_tests)
    add_custom_target(workflow_regression_tests)
endif()
