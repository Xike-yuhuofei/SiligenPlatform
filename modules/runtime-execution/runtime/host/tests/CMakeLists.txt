cmake_minimum_required(VERSION 3.20)

set(RUNTIME_HOST_TEST_SOURCES
    unit/runtime/calibration/CalibrationExecutionServiceTest.cpp
    unit/runtime/diagnostics/DiagnosticsPortAdapterTest.cpp
    unit/runtime/dispensing/DispensingExecutionUseCaseInternalTest.cpp
    unit/runtime/motion/HardLimitMonitorServiceTest.cpp
    unit/runtime/motion/HomingPortAdapterTest.cpp
    unit/runtime/motion/InterpolationAdapterTest.cpp
    unit/runtime/motion/MotionControlMigrationTest.cpp
    unit/runtime/motion/MultiCardMotionAdapterTest.cpp
    unit/runtime/planning/PlanningArtifactExportPortAdapterTest.cpp
    unit/runtime/motion/MotionRuntimeFacadeTest.cpp
    unit/runtime/motion/MotionRuntimeServicesProviderTest.cpp
    unit/runtime/supervision/RuntimeSupervisionPortAdapterTest.cpp
    unit/runtime/system/MachineExecutionStateBackendTest.cpp
    unit/runtime/system/MachineExecutionStateStoreTest.cpp
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
    siligen_runtime_host
    siligen_runtime_execution_application_public
    siligen_application_motion
    siligen_device_adapters
    siligen_device_contracts
    siligen_shared
    siligen_types
    siligen_utils
)

target_include_directories(siligen_runtime_host_unit_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/modules/runtime-execution/application"
)

target_compile_features(siligen_runtime_host_unit_tests PRIVATE cxx_std_17)
target_compile_definitions(siligen_runtime_host_unit_tests PRIVATE SILIGEN_INTERNAL_RUNTIME_EXECUTION_TESTS=1)

add_test(NAME siligen_runtime_host_unit_tests COMMAND siligen_runtime_host_unit_tests)

set(RUNTIME_HOST_REQUIRED_TEST_ROOTS
    "${CMAKE_CURRENT_SOURCE_DIR}/integration/CMakeLists.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/regression/CMakeLists.txt"
)

foreach(RUNTIME_HOST_REQUIRED_TEST_ROOT IN LISTS RUNTIME_HOST_REQUIRED_TEST_ROOTS)
    if(NOT EXISTS "${RUNTIME_HOST_REQUIRED_TEST_ROOT}")
        message(FATAL_ERROR "runtime-host canonical tests root missing: ${RUNTIME_HOST_REQUIRED_TEST_ROOT}")
    endif()
endforeach()

if(NOT TARGET runtime_execution_integration_tests)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/integration"
        "${CMAKE_BINARY_DIR}/tests/runtime-execution-host-integration"
    )
endif()

if(NOT TARGET runtime_execution_regression_tests)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/regression"
        "${CMAKE_BINARY_DIR}/tests/runtime-execution-host-regression"
    )
endif()
