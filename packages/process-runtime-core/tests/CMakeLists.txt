cmake_minimum_required(VERSION 3.20)

set(PROCESS_RUNTIME_CORE_PACKAGE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
if(NOT TARGET siligen_process_runtime_core)
    message(FATAL_ERROR "process-runtime-core tests 需要先加载 siligen_process_runtime_core target")
endif()

set(TEST_SOURCES
    unit/domain/safety/InterlockPolicyTest.cpp
    unit/domain/safety/SoftLimitValidatorTest.cpp
    unit/domain/safety/EmergencyStopServiceTest.cpp
    unit/domain/motion/JogControllerTest.cpp
    unit/domain/dispensing/ArcTriggerPointCalculatorTest.cpp
    unit/domain/dispensing/DispensingControllerTest.cpp
    unit/domain/dispensing/PurgeDispenserProcessTest.cpp
    unit/domain/machine/CalibrationProcessTest.cpp
    unit/domain/recipes/RecipeActivationServiceTest.cpp
    unit/domain/motion/MotionBufferControllerTest.cpp
    unit/recipes/RecipeCommandUseCaseTest.cpp
    unit/motion/HomeAxesUseCaseTest.cpp
    unit/motion/HomingSupportTest.cpp
    unit/motion/MotionMonitoringUseCaseTest.cpp
    unit/motion/MotionSafetyUseCaseTest.cpp
    unit/system/InitializeSystemUseCaseTest.cpp
    unit/motion/MotionCoordinationUseCaseTest.cpp
    unit/dispensing/CleanupFilesUseCaseTest.cpp
    unit/dispensing/DispensingWorkflowUseCaseTest.cpp
    unit/dispensing/DispensingExecutionUseCaseProgressTest.cpp
    unit/dispensing/DispensingProcessServiceWaitForMotionCompleteTest.cpp
)

set(OPTIONAL_TEST_SOURCES
    unit/domain/diagnostics/ProcessResultServiceTest.cpp
    unit/domain/dispensing/TriggerPlannerTest.cpp
    unit/domain/dispensing/ClosedLoopSkeletonTest.cpp
    unit/domain/trajectory/MotionPlannerTest.cpp
    unit/domain/trajectory/MotionPlannerConstraintTest.cpp
    unit/domain/trajectory/InterpolationProgramPlannerTest.cpp
    unit/domain/trajectory/PathRegularizerTest.cpp
    unit/dispensing/PlanningRequestTest.cpp
    unit/dispensing/ContourPathOptimizerTest.cpp
    unit/dispensing/UploadFileUseCaseTest.cpp
    unit/infrastructure/adapters/planning/dxf/DXFAdapterFactoryTest.cpp
    unit/infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapterTest.cpp
    unit/redundancy/CandidateScoringServiceTest.cpp
    unit/redundancy/RedundancyGovernanceUseCasesTest.cpp
)

foreach(test_source IN LISTS OPTIONAL_TEST_SOURCES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test_source}")
        list(APPEND TEST_SOURCES "${test_source}")
    endif()
endforeach()

add_executable(siligen_unit_tests ${TEST_SOURCES})

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_unit_tests PRIVATE
    siligen_process_runtime_core
    domain_dispensing
    siligen_parsing_adapter
    siligen_types
    siligen_utils
)

target_include_directories(siligen_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/packages/device-adapters/src
    ${CMAKE_SOURCE_DIR}/packages/device-adapters/include
)

process_runtime_core_add_test_with_runtime_environment(
    siligen_unit_tests
    siligen_unit_tests
)

set(PR1_TEST_SOURCES
    unit/domain/safety/EmergencyStopServiceTest.cpp
    unit/domain/motion/MotionBufferControllerTest.cpp
    unit/motion/MotionCoordinationUseCaseTest.cpp
)

add_executable(siligen_pr1_tests ${PR1_TEST_SOURCES})

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_pr1_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_pr1_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_pr1_tests PRIVATE
    siligen_process_runtime_core
    siligen_types
    siligen_utils
)

process_runtime_core_add_test_with_runtime_environment(
    siligen_pr1_tests
    siligen_pr1_tests
)

if(NOT TARGET process_runtime_core_tests)
    add_custom_target(process_runtime_core_tests
        DEPENDS
            siligen_unit_tests
            siligen_pr1_tests
    )
endif()
