cmake_minimum_required(VERSION 3.20)

foreach(required_target
    IN ITEMS
        siligen_workflow_application_headers
        siligen_workflow_contracts_public
        siligen_application_dispensing
        siligen_process_planning_contracts_public
        siligen_process_path_contracts_public
        siligen_motion_planning_contracts_public
        siligen_shared_runtime_contracts
        siligen_trace_diagnostics_contracts_public
        siligen_types
        siligen_utils
)
    if(NOT TARGET ${required_target})
        message(FATAL_ERROR "workflow integration tests 缺少必需 target: ${required_target}")
    endif()
endforeach()

set(WORKFLOW_INTEGRATION_TEST_SOURCES
    DispensingWorkflowUseCaseTest.cpp
    PlanningUseCaseExportPortTest.cpp
    PlanningFailureSurfaceTest.cpp
)

add_executable(siligen_dispensing_semantics_tests
    ${WORKFLOW_INTEGRATION_TEST_SOURCES}
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_dispensing_semantics_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_dispensing_semantics_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_include_directories(siligen_dispensing_semantics_tests PRIVATE
    ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_dispensing_semantics_tests PRIVATE
    siligen_workflow_application_headers
    siligen_workflow_contracts_public
    siligen_application_dispensing
    siligen_process_planning_contracts_public
    siligen_process_path_contracts_public
    siligen_motion_planning_contracts_public
    siligen_shared_runtime_contracts
    siligen_trace_diagnostics_contracts_public
    siligen_types
    siligen_utils
)
if(TARGET siligen_runtime_execution_application_public)
    target_link_libraries(siligen_dispensing_semantics_tests PRIVATE
        siligen_runtime_execution_application_public
    )
endif()
if(TARGET siligen_dispense_packaging_application_public)
    target_link_libraries(siligen_dispensing_semantics_tests PRIVATE
        siligen_dispense_packaging_application_public
    )
endif()
if(TARGET siligen_process_path_application_public)
    target_link_libraries(siligen_dispensing_semantics_tests PRIVATE
        siligen_process_path_application_public
    )
endif()
if(TARGET siligen_motion_planning_application_public)
    target_link_libraries(siligen_dispensing_semantics_tests PRIVATE
        siligen_motion_planning_application_public
    )
endif()
if(TARGET siligen_job_ingest_application_public)
    target_link_libraries(siligen_dispensing_semantics_tests PRIVATE
        siligen_job_ingest_application_public
    )
endif()
if(TARGET siligen_dxf_geometry_application_public)
    target_link_libraries(siligen_dispensing_semantics_tests PRIVATE
        siligen_dxf_geometry_application_public
    )
endif()
target_compile_definitions(siligen_dispensing_semantics_tests PRIVATE SILIGEN_TEST_HOOKS=1)
target_compile_features(siligen_dispensing_semantics_tests PRIVATE cxx_std_17)

siligen_add_test_with_runtime_environment(
    siligen_dispensing_semantics_tests
    siligen_dispensing_semantics_tests
)

if(NOT TARGET workflow_integration_tests)
    add_custom_target(workflow_integration_tests
        DEPENDS siligen_dispensing_semantics_tests
    )
else()
    add_dependencies(workflow_integration_tests
        siligen_dispensing_semantics_tests
    )
endif()
