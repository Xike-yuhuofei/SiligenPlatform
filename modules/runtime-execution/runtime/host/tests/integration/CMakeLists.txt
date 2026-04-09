cmake_minimum_required(VERSION 3.20)

set(RUNTIME_HOST_INTEGRATION_TEST_SOURCES
    HostBootstrapSmokeTest.cpp
    RuntimeExecutionSupervisionIntegrationTest.cpp
)

add_executable(runtime_execution_integration_host_bootstrap_smoke
    ${RUNTIME_HOST_INTEGRATION_TEST_SOURCES}
)

if(TARGET GTest::gtest_main)
    target_link_libraries(runtime_execution_integration_host_bootstrap_smoke PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(runtime_execution_integration_host_bootstrap_smoke PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_include_directories(runtime_execution_integration_host_bootstrap_smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(runtime_execution_integration_host_bootstrap_smoke PRIVATE
    siligen_runtime_host
    siligen_device_adapters
    siligen_device_contracts
    siligen_shared
    siligen_types
    siligen_utils
)

target_compile_features(runtime_execution_integration_host_bootstrap_smoke PRIVATE cxx_std_17)
target_compile_definitions(runtime_execution_integration_host_bootstrap_smoke PRIVATE
    SILIGEN_INTERNAL_RUNTIME_EXECUTION_TESTS=1
)

add_test(
    NAME runtime_execution_integration_host_bootstrap_smoke
    COMMAND runtime_execution_integration_host_bootstrap_smoke
)

if(NOT TARGET runtime_execution_integration_tests)
    add_custom_target(runtime_execution_integration_tests)
endif()

add_dependencies(runtime_execution_integration_tests
    runtime_execution_integration_host_bootstrap_smoke
)
