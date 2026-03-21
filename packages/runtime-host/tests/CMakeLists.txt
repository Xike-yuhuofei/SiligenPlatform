cmake_minimum_required(VERSION 3.20)

set(RUNTIME_HOST_TEST_SOURCES
    unit/runtime/configuration/InterlockConfigResolverTest.cpp
    unit/runtime/configuration/ConfigFileAdapterHardwareConfigurationTest.cpp
)

add_executable(siligen_runtime_host_unit_tests ${RUNTIME_HOST_TEST_SOURCES})

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_runtime_host_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_runtime_host_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_runtime_host_unit_tests PRIVATE
    siligen_control_runtime_config
    siligen_types
    siligen_utils
)

target_compile_features(siligen_runtime_host_unit_tests PRIVATE cxx_std_17)

add_test(NAME siligen_runtime_host_unit_tests COMMAND siligen_runtime_host_unit_tests)
