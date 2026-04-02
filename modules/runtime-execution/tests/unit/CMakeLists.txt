cmake_minimum_required(VERSION 3.20)

set(RUNTIME_EXECUTION_UNIT_TEST_SOURCES
    DispensingExecutionUseCaseInternalTest.cpp
)

add_executable(siligen_runtime_execution_unit_tests
    ${RUNTIME_EXECUTION_UNIT_TEST_SOURCES}
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_runtime_execution_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_runtime_execution_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_runtime_execution_unit_tests PRIVATE
    siligen_runtime_execution_application_public
    siligen_domain
    siligen_shared
    siligen_types
    siligen_utils
)

target_include_directories(siligen_runtime_execution_unit_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/modules/runtime-execution/application"
)

target_compile_features(siligen_runtime_execution_unit_tests PRIVATE cxx_std_17)
target_compile_definitions(siligen_runtime_execution_unit_tests PRIVATE SILIGEN_INTERNAL_RUNTIME_EXECUTION_TESTS=1)

add_test(NAME siligen_runtime_execution_unit_tests COMMAND siligen_runtime_execution_unit_tests)
