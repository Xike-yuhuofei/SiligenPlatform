cmake_minimum_required(VERSION 3.20)

set(WORKFLOW_OWNER_UNIT_TEST_SOURCES
    MotionOwnerBehaviorTest.cpp
    PlanningFailureSurfaceTest.cpp
)

add_executable(workflow_owner_unit_tests ${WORKFLOW_OWNER_UNIT_TEST_SOURCES})

if(TARGET GTest::gtest_main)
    target_link_libraries(workflow_owner_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(workflow_owner_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(workflow_owner_unit_tests PRIVATE
    siligen_application_dispensing
    siligen_application_motion
    siligen_workflow_contracts_public
    siligen_dispense_packaging_application_public
    siligen_dxf_geometry_application_public
    siligen_motion_planning_application_public
    siligen_process_path_application_public
    siligen_runtime_execution_application_public
    siligen_types
    siligen_utils
)

target_include_directories(workflow_owner_unit_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../../application"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../application/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../domain/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../adapters/include"
    "${CMAKE_SOURCE_DIR}/modules/runtime-execution/application/include"
)

target_compile_definitions(workflow_owner_unit_tests PRIVATE SILIGEN_TEST_HOOKS=1)
target_compile_features(workflow_owner_unit_tests PRIVATE cxx_std_17)

siligen_add_test_with_runtime_environment(
    workflow_owner_unit_tests
    workflow_owner_unit_tests
)
