cmake_minimum_required(VERSION 3.20)

set(WORKFLOW_INTEGRATION_FORBIDDEN_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/PlanningUseCaseExportPortTest.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PlanningFailureSurfaceTest.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DispensingWorkflowUseCaseTest.cpp"
)
foreach(forbidden_source IN LISTS WORKFLOW_INTEGRATION_FORBIDDEN_SOURCES)
    if(EXISTS "${forbidden_source}")
        message(FATAL_ERROR "workflow integration tests must not host foreign-owner sources: ${forbidden_source}")
    endif()
endforeach()

if(NOT TARGET workflow_integration_tests)
    add_custom_target(workflow_integration_tests)
endif()

message(STATUS "[Workflow::Tests] owner-owned planning/execution integration tests moved out of workflow/tests")
