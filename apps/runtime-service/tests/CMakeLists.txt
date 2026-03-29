cmake_minimum_required(VERSION 3.20)

set(RUNTIME_PROCESS_BOOTSTRAP_UNIT_TEST_SOURCES
    unit/runtime/configuration/ConfigFileAdapterHardwareConfigurationTest.cpp
    unit/runtime/configuration/InterlockConfigResolverTest.cpp
    unit/runtime/configuration/WorkspaceAssetPathsTest.cpp
    unit/runtime/recipes/TemplateFileRepositoryTest.cpp
)

add_executable(siligen_runtime_service_unit_tests
    ${RUNTIME_PROCESS_BOOTSTRAP_UNIT_TEST_SOURCES}
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_runtime_service_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_runtime_service_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_include_directories(siligen_runtime_service_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_runtime_service_unit_tests PRIVATE
    siligen_runtime_process_bootstrap
)
target_compile_features(siligen_runtime_service_unit_tests PRIVATE cxx_std_17)

add_test(NAME siligen_runtime_service_unit_tests COMMAND siligen_runtime_service_unit_tests)

add_executable(runtime_service_integration_host_bootstrap_smoke
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/HostBootstrapSmokeTest.cpp
)

if(TARGET GTest::gtest_main)
    target_link_libraries(runtime_service_integration_host_bootstrap_smoke PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(runtime_service_integration_host_bootstrap_smoke PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(runtime_service_integration_host_bootstrap_smoke PRIVATE
    siligen_runtime_process_bootstrap_public
)
target_compile_features(runtime_service_integration_host_bootstrap_smoke PRIVATE cxx_std_17)

add_test(
    NAME runtime_service_integration_host_bootstrap_smoke
    COMMAND runtime_service_integration_host_bootstrap_smoke
)
