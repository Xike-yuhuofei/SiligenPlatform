cmake_minimum_required(VERSION 3.20)

set(RUNTIME_HOST_REGRESSION_TEST_SOURCES
    RuntimeExecutionStateMachineRegressionTest.cpp
    RuntimeExecutionRecoveryRegressionTest.cpp
)

add_executable(runtime_execution_regression_host_core
    ${RUNTIME_HOST_REGRESSION_TEST_SOURCES}
)

target_include_directories(runtime_execution_regression_host_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

if(TARGET GTest::gtest_main)
    target_link_libraries(runtime_execution_regression_host_core PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(runtime_execution_regression_host_core PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(runtime_execution_regression_host_core PRIVATE
    siligen_runtime_host
    siligen_device_adapters
    siligen_device_contracts
    siligen_workflow_domain_headers
    siligen_shared
    siligen_types
    siligen_utils
)

target_compile_features(runtime_execution_regression_host_core PRIVATE cxx_std_17)
target_compile_definitions(runtime_execution_regression_host_core PRIVATE
    SILIGEN_INTERNAL_RUNTIME_EXECUTION_TESTS=1
)

add_test(
    NAME runtime_execution_regression_host_core
    COMMAND runtime_execution_regression_host_core
)

if(NOT TARGET runtime_execution_regression_tests)
    add_custom_target(runtime_execution_regression_tests)
endif()

add_dependencies(runtime_execution_regression_tests
    runtime_execution_regression_host_core
)
