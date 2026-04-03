cmake_minimum_required(VERSION 3.20)

set(PROCESS_PATH_TEST_SOURCES
    contract/ProcessPathContractsTest.cpp
    golden/ProcessPathBuildGoldenTest.cpp
    integration/ProcessPathToMotionPlanningIntegrationTest.cpp
    unit/domain/trajectory/PathRegularizerTest.cpp
    unit/application/services/process_path/ProcessPathFacadeTest.cpp
)

add_executable(siligen_process_path_unit_tests ${PROCESS_PATH_TEST_SOURCES})

target_include_directories(siligen_process_path_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_process_path_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_process_path_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_process_path_unit_tests PRIVATE
    siligen_process_path_application_public
    siligen_process_path_domain_trajectory
    siligen_parsing_adapter
    siligen_types
    siligen_utils
)
if(TARGET siligen_motion_planning_application_public)
    target_link_libraries(siligen_process_path_unit_tests PRIVATE
        siligen_motion_planning_application_public
    )
else()
    message(FATAL_ERROR "process-path tests require siligen_motion_planning_application_public for integration coverage")
endif()
target_compile_features(siligen_process_path_unit_tests PRIVATE cxx_std_17)

if(COMMAND process_runtime_core_add_test_with_runtime_environment)
    process_runtime_core_add_test_with_runtime_environment(
        siligen_process_path_unit_tests
        siligen_process_path_unit_tests
    )
else()
    add_test(NAME siligen_process_path_unit_tests COMMAND siligen_process_path_unit_tests)
endif()
