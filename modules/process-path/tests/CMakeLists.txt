cmake_minimum_required(VERSION 3.20)

set(PROCESS_PATH_TEST_SOURCES
    unit/domain/trajectory/PathRegularizerTest.cpp
    unit/application/services/process_path/ProcessPathFacadeTest.cpp
)

add_executable(siligen_process_path_unit_tests ${PROCESS_PATH_TEST_SOURCES})

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_process_path_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_process_path_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_process_path_unit_tests PRIVATE
    siligen_process_path_application_public
    siligen_types
    siligen_utils
)
target_compile_features(siligen_process_path_unit_tests PRIVATE cxx_std_17)

if(COMMAND process_runtime_core_add_test_with_runtime_environment)
    process_runtime_core_add_test_with_runtime_environment(
        siligen_process_path_unit_tests
        siligen_process_path_unit_tests
    )
else()
    add_test(NAME siligen_process_path_unit_tests COMMAND siligen_process_path_unit_tests)
endif()
