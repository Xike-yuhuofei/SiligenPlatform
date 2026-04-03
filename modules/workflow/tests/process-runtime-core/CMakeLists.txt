cmake_minimum_required(VERSION 3.20)

set(WORKFLOW_MODULE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)
if(NOT TARGET siligen_process_runtime_core)
    message(FATAL_ERROR "workflow canonical tests 需要先加载 siligen_process_runtime_core target")
endif()

set(WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS
    "${WORKFLOW_MODULE_DIR}"
    "${WORKFLOW_MODULE_DIR}/domain"
    "${WORKFLOW_MODULE_DIR}/application"
    "${WORKFLOW_MODULE_DIR}/adapters"
    "${WORKFLOW_MODULE_DIR}/domain/process-core/include"
    "${WORKFLOW_MODULE_DIR}/domain/motion-core/include"
    "${WORKFLOW_MODULE_DIR}/application/include"
    "${WORKFLOW_MODULE_DIR}/domain/include"
    "${WORKFLOW_MODULE_DIR}/adapters/include"
    "${CMAKE_SOURCE_DIR}/modules/job-ingest/contracts/include"
    "${CMAKE_SOURCE_DIR}/modules/runtime-execution/application/include"
    "${CMAKE_SOURCE_DIR}/modules/motion-planning"
    "${CMAKE_SOURCE_DIR}/modules/runtime-execution/adapters/device/include"
)

set(TEST_SOURCES
    unit/domain/safety/InterlockPolicyTest.cpp
    unit/domain/safety/SoftLimitValidatorTest.cpp
    unit/domain/safety/EmergencyStopServiceTest.cpp
    unit/domain/safety/SafetyOutputGuardTest.cpp
    unit/domain/motion/JogControllerTest.cpp
    unit/domain/dispensing/ArcTriggerPointCalculatorTest.cpp
    unit/domain/trajectory/TrajectoryShaperTest.cpp
    unit/domain/dispensing/PurgeDispenserProcessTest.cpp
    unit/domain/recipes/RecipeActivationServiceTest.cpp
    unit/domain/motion/MotionBufferControllerTest.cpp
    unit/recipes/RecipeCommandUseCaseTest.cpp
    unit/motion/HomeAxesUseCaseTest.cpp
    unit/motion/EnsureAxesReadyZeroUseCaseTest.cpp
    unit/motion/HomingSupportTest.cpp
    unit/motion/MotionMonitoringUseCaseTest.cpp
    unit/motion/MotionSafetyUseCaseTest.cpp
    unit/system/InitializeSystemUseCaseTest.cpp
    unit/motion/MotionCoordinationUseCaseTest.cpp
    unit/dispensing/CleanupFilesUseCaseTest.cpp
    unit/dispensing/DispensingWorkflowUseCaseTest.cpp
    unit/dispensing/DispensingWorkflowModeResolutionTest.cpp
    unit/dispensing/DispensingProcessServiceWaitForMotionCompleteTest.cpp
    unit/dispensing/PlanningUseCaseExportPortTest.cpp
)

set(OPTIONAL_TEST_SOURCES
    unit/dispensing/DispensingProcessServiceBufferingTest.cpp
    unit/domain/diagnostics/ProcessResultServiceTest.cpp
    unit/domain/dispensing/ClosedLoopSkeletonTest.cpp
    unit/domain/dispensing/ValveCoordinationServiceTest.cpp
    unit/dispensing/DispensingProcessServiceBufferingTest.cpp
    unit/dispensing/PlanningRequestTest.cpp
    unit/dispensing/PlanningUseCaseExportPortTest.cpp
    unit/dispensing/ContourPathOptimizerTest.cpp
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
    siligen_dispense_packaging_application_public
    siligen_parsing_adapter
    siligen_types
    siligen_utils
)
if(TARGET siligen_application_dispensing)
    target_link_libraries(siligen_unit_tests PRIVATE
        siligen_application_dispensing
    )
endif()
if(TARGET siligen_process_path_application_public)
    target_link_libraries(siligen_unit_tests PRIVATE
        siligen_process_path_application_public
    )
endif()
if(TARGET siligen_motion_planning_application_public)
    target_link_libraries(siligen_unit_tests PRIVATE
        siligen_motion_planning_application_public
    )
endif()
if(TARGET siligen_workflow_dispensing_planning_compat)
    target_link_libraries(siligen_unit_tests PRIVATE
        siligen_workflow_dispensing_planning_compat
    )
endif()
if(TARGET siligen_runtime_execution_application_public)
    target_link_libraries(siligen_unit_tests PRIVATE
        siligen_runtime_execution_application_public
    )
endif()
if(TARGET siligen_job_ingest_application_public)
    target_link_libraries(siligen_unit_tests PRIVATE
        siligen_job_ingest_application_public
    )
endif()
if(TARGET siligen_dxf_geometry_application_public)
    target_link_libraries(siligen_unit_tests PRIVATE
        siligen_dxf_geometry_application_public
    )
endif()
target_compile_definitions(siligen_unit_tests PRIVATE SILIGEN_TEST_HOOKS=1)

target_include_directories(siligen_unit_tests PRIVATE
    ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
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
target_compile_definitions(siligen_pr1_tests PRIVATE SILIGEN_TEST_HOOKS=1)
target_include_directories(siligen_pr1_tests PRIVATE
    ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
)

process_runtime_core_add_test_with_runtime_environment(
    siligen_pr1_tests
    siligen_pr1_tests
)

set(DISPENSING_SEMANTICS_TEST_SOURCES
    unit/domain/safety/SafetyOutputGuardTest.cpp
    unit/dispensing/DispensingWorkflowUseCaseTest.cpp
    unit/dispensing/DispensingWorkflowModeResolutionTest.cpp
    unit/dispensing/DispensingProcessServiceWaitForMotionCompleteTest.cpp
    unit/dispensing/PlanningUseCaseExportPortTest.cpp
)

add_executable(siligen_dispensing_semantics_tests ${DISPENSING_SEMANTICS_TEST_SOURCES})

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_dispensing_semantics_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_dispensing_semantics_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_dispensing_semantics_tests PRIVATE
    siligen_process_runtime_core
    siligen_application_dispensing
    siligen_runtime_execution_application_public
    siligen_job_ingest_application_public
    siligen_dxf_geometry_application_public
    siligen_dispense_packaging_application_public
    siligen_parsing_adapter
    siligen_types
    siligen_utils
)
if(TARGET siligen_process_path_application_public)
    target_link_libraries(siligen_dispensing_semantics_tests PRIVATE
        siligen_process_path_application_public
    )
endif()
if(TARGET siligen_motion_planning_application_public)
    target_link_libraries(siligen_dispensing_semantics_tests PRIVATE
        siligen_motion_planning_application_public
    )
endif()
target_compile_definitions(siligen_dispensing_semantics_tests PRIVATE SILIGEN_TEST_HOOKS=1)
target_include_directories(siligen_dispensing_semantics_tests PRIVATE
    ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
)

process_runtime_core_add_test_with_runtime_environment(
    siligen_dispensing_semantics_tests
    siligen_dispensing_semantics_tests
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/motion/DeterministicPathExecutionUseCaseTest.cpp")
    add_executable(process_runtime_core_deterministic_path_execution_test
        unit/motion/DeterministicPathExecutionUseCaseTest.cpp
    )
    target_include_directories(process_runtime_core_deterministic_path_execution_test PRIVATE
        ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
    )
    target_link_libraries(process_runtime_core_deterministic_path_execution_test PRIVATE
        siligen_application_motion
        siligen_types
        siligen_utils
    )
    target_compile_features(process_runtime_core_deterministic_path_execution_test PRIVATE cxx_std_17)
    process_runtime_core_add_test_with_runtime_environment(
        process_runtime_core_deterministic_path_execution_test
        process_runtime_core_deterministic_path_execution_test
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/motion/MotionRuntimeAssemblyFactoryTest.cpp")
    add_executable(process_runtime_core_motion_runtime_assembly_test
        unit/motion/MotionRuntimeAssemblyFactoryTest.cpp
    )
    target_include_directories(process_runtime_core_motion_runtime_assembly_test PRIVATE
        ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
    )
    target_link_libraries(process_runtime_core_motion_runtime_assembly_test PRIVATE
        siligen_application_motion
        siligen_types
        siligen_utils
    )
    target_compile_features(process_runtime_core_motion_runtime_assembly_test PRIVATE cxx_std_17)
    process_runtime_core_add_test_with_runtime_environment(
        process_runtime_core_motion_runtime_assembly_test
        process_runtime_core_motion_runtime_assembly_test
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/infrastructure/adapters/planning/dxf/PbPathSourceAdapterContractTest.cpp")
    add_executable(process_runtime_core_pb_path_source_adapter_test
        unit/infrastructure/adapters/planning/dxf/PbPathSourceAdapterContractTest.cpp
    )
    target_include_directories(process_runtime_core_pb_path_source_adapter_test PRIVATE
        ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
    )
    target_link_libraries(process_runtime_core_pb_path_source_adapter_test PRIVATE
        siligen_parsing_adapter
        siligen_types
        siligen_utils
    )
    target_compile_features(process_runtime_core_pb_path_source_adapter_test PRIVATE cxx_std_17)
    process_runtime_core_add_test_with_runtime_environment(
        PbPathSourceAdapterContractTest
        process_runtime_core_pb_path_source_adapter_test
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/infrastructure/adapters/planning/dxf/DXFAdapterFactoryTest.cpp")
    add_executable(process_runtime_core_dxf_adapter_factory_test
        unit/infrastructure/adapters/planning/dxf/DXFAdapterFactoryTest.cpp
    )
    target_include_directories(process_runtime_core_dxf_adapter_factory_test PRIVATE
        ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
    )
    if(TARGET GTest::gtest_main)
        target_link_libraries(process_runtime_core_dxf_adapter_factory_test PRIVATE GTest::gtest_main)
    elseif(TARGET gtest_main)
        target_link_libraries(process_runtime_core_dxf_adapter_factory_test PRIVATE gtest_main)
    else()
        message(FATAL_ERROR "GoogleTest target not found")
    endif()
    target_link_libraries(process_runtime_core_dxf_adapter_factory_test PRIVATE
        siligen_parsing_adapter
        siligen_types
        siligen_utils
    )
    target_compile_features(process_runtime_core_dxf_adapter_factory_test PRIVATE cxx_std_17)
    process_runtime_core_add_test_with_runtime_environment(
        DXFAdapterFactoryTest
        process_runtime_core_dxf_adapter_factory_test
    )
endif()

if(NOT TARGET process_runtime_core_tests)
    add_custom_target(process_runtime_core_tests
        DEPENDS
            siligen_unit_tests
            siligen_pr1_tests
            siligen_dispensing_semantics_tests
    )
endif()

if(TARGET process_runtime_core_tests)
    if(TARGET process_runtime_core_deterministic_path_execution_test)
        add_dependencies(process_runtime_core_tests
            process_runtime_core_deterministic_path_execution_test
        )
    endif()
    if(TARGET process_runtime_core_motion_runtime_assembly_test)
        add_dependencies(process_runtime_core_tests
            process_runtime_core_motion_runtime_assembly_test
        )
    endif()
    if(TARGET process_runtime_core_pb_path_source_adapter_test)
        add_dependencies(process_runtime_core_tests
            process_runtime_core_pb_path_source_adapter_test
        )
    endif()
    if(TARGET process_runtime_core_dxf_adapter_factory_test)
        add_dependencies(process_runtime_core_tests
            process_runtime_core_dxf_adapter_factory_test
        )
    endif()
endif()
