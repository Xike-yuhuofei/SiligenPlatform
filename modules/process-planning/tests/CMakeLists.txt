cmake_minimum_required(VERSION 3.20)

set(PROCESS_PLANNING_TEST_SOURCES
    unit/ReadyZeroSpeedResolverTest.cpp
    contract/ConfigurationContractsTest.cpp
    golden/ConfigurationDefaultsGoldenTest.cpp
)

add_executable(siligen_process_planning_tests ${PROCESS_PLANNING_TEST_SOURCES})

target_include_directories(siligen_process_planning_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_process_planning_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_process_planning_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_process_planning_tests PRIVATE
    siligen_module_process_planning
    siligen_process_planning_contracts_public
    siligen_types
)
target_compile_features(siligen_process_planning_tests PRIVATE cxx_std_17)

add_test(NAME siligen_process_planning_tests COMMAND siligen_process_planning_tests)
