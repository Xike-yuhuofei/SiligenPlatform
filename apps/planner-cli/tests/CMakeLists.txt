cmake_minimum_required(VERSION 3.20)

add_executable(siligen_planner_cli_unit_tests
    unit/CommandLineParserTest.cpp
    ../CommandLineParser.cpp
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_planner_cli_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_planner_cli_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_include_directories(siligen_planner_cli_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_planner_cli_unit_tests PRIVATE
    siligen_runtime_process_bootstrap_public
    siligen_shared
)
target_compile_features(siligen_planner_cli_unit_tests PRIVATE cxx_std_17)

add_test(NAME siligen_planner_cli_unit_tests COMMAND siligen_planner_cli_unit_tests)
