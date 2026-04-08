cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_workflow_application_public)
    message(FATAL_ERROR "workflow integration tests 需要先加载 siligen_workflow_application_public target")
endif()

set(WORKFLOW_INTEGRATION_TEST_SOURCES
    DispensingWorkflowUseCaseTest.cpp
    PlanningUseCaseExportPortTest.cpp
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
    siligen_workflow_application_public
    siligen_types
    siligen_utils
)
if(TARGET siligen_application_dispensing)
    target_link_libraries(siligen_dispensing_semantics_tests PRIVATE
        siligen_application_dispensing
    )
endif()
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
if(TARGET siligen_parsing_adapter)
    target_link_libraries(siligen_dispensing_semantics_tests PRIVATE
        siligen_parsing_adapter
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
