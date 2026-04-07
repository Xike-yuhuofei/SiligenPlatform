cmake_minimum_required(VERSION 3.20)

set(RUNTIME_PROCESS_BOOTSTRAP_UNIT_TEST_SOURCES
    unit/runtime/configuration/ConfigValidatorTest.cpp
    unit/runtime/configuration/ConfigFileAdapterHardwareConfigurationTest.cpp
    unit/runtime/configuration/InterlockConfigResolverTest.cpp
    unit/runtime/configuration/WorkspaceAssetPathsTest.cpp
    unit/runtime/recipes/TemplateFileRepositoryTest.cpp
    unit/runtime/status/RuntimeStatusExportPortTest.cpp
    unit/runtime/supervision/RuntimeExecutionSupervisionBackendTest.cpp
    unit/runtime/supervision/RuntimeSupervisionSyncPortTest.cpp
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
target_compile_definitions(siligen_runtime_service_unit_tests PRIVATE
    SILIGEN_TEST_HOOKS=1
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

if(EXISTS "${SILIGEN_MULTICARD_VENDOR_DIR}/MultiCard.dll")
    add_custom_command(TARGET runtime_service_integration_host_bootstrap_smoke POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SILIGEN_MULTICARD_VENDOR_DIR}/MultiCard.dll"
            "$<TARGET_FILE_DIR:runtime_service_integration_host_bootstrap_smoke>"
        COMMENT "拷贝 MultiCard.dll 到 runtime-service smoke 输出目录"
    )
endif()

add_test(
    NAME runtime_service_integration_host_bootstrap_smoke
    COMMAND runtime_service_integration_host_bootstrap_smoke
)
