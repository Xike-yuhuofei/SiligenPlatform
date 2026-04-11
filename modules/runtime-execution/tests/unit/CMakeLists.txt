cmake_minimum_required(VERSION 3.20)

set(RUNTIME_EXECUTION_TEST_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/modules/runtime-execution"
    "${CMAKE_SOURCE_DIR}/modules/runtime-execution/application"
    "${CMAKE_SOURCE_DIR}/modules/dispense-packaging"
    "${CMAKE_SOURCE_DIR}/modules/motion-planning/contracts/include"
)

set(RUNTIME_EXECUTION_UNIT_TEST_SOURCES
    DispensingExecutionUseCaseInternalTest.cpp
    DispensingProcessPortFactoryTest.cpp
    DispensingProcessServiceTraceTest.cpp
    DispensingProcessServiceWaitForMotionCompleteTest.cpp
    domain/motion/JogControllerTest.cpp
    domain/motion/MotionBufferControllerTest.cpp
    domain/safety/EmergencyStopServiceTest.cpp
    domain/safety/InterlockPolicyTest.cpp
    domain/safety/SafetyOutputGuardTest.cpp
    domain/safety/SoftLimitValidatorTest.cpp
    motion/EnsureAxesReadyZeroUseCaseTest.cpp
    motion/HomeAxesUseCaseTest.cpp
    motion/HomingSupportTest.cpp
    motion/MotionCoordinationUseCaseTest.cpp
    motion/MotionMonitoringUseCaseTest.cpp
    motion/MotionSafetyUseCaseTest.cpp
    system/InitializeSystemUseCaseTest.cpp
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
    siligen_shared
    siligen_types
    siligen_utils
)
if(TARGET siligen_device_adapters)
    target_link_libraries(siligen_runtime_execution_unit_tests PRIVATE
        siligen_device_adapters
    )
endif()

target_include_directories(siligen_runtime_execution_unit_tests PRIVATE
    ${RUNTIME_EXECUTION_TEST_INCLUDE_DIRS}
)

target_compile_features(siligen_runtime_execution_unit_tests PRIVATE cxx_std_17)
target_compile_definitions(siligen_runtime_execution_unit_tests PRIVATE SILIGEN_INTERNAL_RUNTIME_EXECUTION_TESTS=1)

add_test(NAME siligen_runtime_execution_unit_tests COMMAND siligen_runtime_execution_unit_tests)

add_executable(runtime_execution_motion_runtime_assembly_test
    motion/MotionRuntimeAssemblyFactoryTest.cpp
)
target_include_directories(runtime_execution_motion_runtime_assembly_test PRIVATE
    ${RUNTIME_EXECUTION_TEST_INCLUDE_DIRS}
)
target_link_libraries(runtime_execution_motion_runtime_assembly_test PRIVATE
    siligen_application_motion
    siligen_runtime_execution_motion_execution_services
    siligen_types
    siligen_utils
)
target_compile_features(runtime_execution_motion_runtime_assembly_test PRIVATE cxx_std_17)
add_test(
    NAME runtime_execution_motion_runtime_assembly_test
    COMMAND runtime_execution_motion_runtime_assembly_test
)
