cmake_minimum_required(VERSION 3.20)

set(WORKFLOW_MODULE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

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
    "${CMAKE_SOURCE_DIR}/modules/motion-planning"
)

set(TEST_SOURCES
    domain/safety/InterlockPolicyTest.cpp
    domain/safety/SoftLimitValidatorTest.cpp
    domain/safety/EmergencyStopServiceTest.cpp
    domain/safety/SafetyOutputGuardTest.cpp
    domain/dispensing/ArcTriggerPointCalculatorTest.cpp
    domain/trajectory/TrajectoryShaperTest.cpp
    domain/dispensing/PurgeDispenserProcessTest.cpp
    domain/recipes/RecipeActivationServiceTest.cpp
    recipes/RecipeCommandUseCaseTest.cpp
    motion/MotionSafetyUseCaseTest.cpp
    system/InitializeSystemUseCaseTest.cpp
    system/WorkflowBoundaryCutoverTest.cpp
    motion/MotionCoordinationUseCaseTest.cpp
    dispensing/CleanupFilesUseCaseTest.cpp
    dispensing/DispensingWorkflowModeResolutionTest.cpp
    dispensing/DispensingProcessServiceWaitForMotionCompleteTest.cpp
    dispensing/PlanningUseCaseExportPortTest.cpp
)

set(OPTIONAL_TEST_SOURCES
    domain/diagnostics/ProcessResultServiceTest.cpp
    domain/dispensing/ClosedLoopSkeletonTest.cpp
    domain/dispensing/ValveCoordinationServiceTest.cpp
    dispensing/PlanningRequestTest.cpp
    dispensing/PlanningUseCaseExportPortTest.cpp
    infrastructure/adapters/planning/dxf/DXFAdapterFactoryTest.cpp
    infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapterTest.cpp
    redundancy/CandidateScoringServiceTest.cpp
    redundancy/RedundancyGovernanceUseCasesTest.cpp
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
    siligen_workflow_contracts_public
    siligen_workflow_domain_public
    siligen_workflow_application_public
    siligen_workflow_recipe_application_public
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

siligen_add_test_with_runtime_environment(
    siligen_unit_tests
    siligen_unit_tests
)

set(PR1_TEST_SOURCES
    domain/safety/EmergencyStopServiceTest.cpp
    motion/MotionCoordinationUseCaseTest.cpp
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
    siligen_workflow_contracts_public
    siligen_workflow_domain_public
    siligen_workflow_application_public
    siligen_types
    siligen_utils
)
target_compile_definitions(siligen_pr1_tests PRIVATE SILIGEN_TEST_HOOKS=1)
target_include_directories(siligen_pr1_tests PRIVATE
    ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
)

siligen_add_test_with_runtime_environment(
    siligen_pr1_tests
    siligen_pr1_tests
)

set(DISPENSING_SEMANTICS_TEST_SOURCES
    domain/safety/SafetyOutputGuardTest.cpp
    dispensing/DispensingWorkflowModeResolutionTest.cpp
    dispensing/DispensingProcessServiceWaitForMotionCompleteTest.cpp
    dispensing/PlanningUseCaseExportPortTest.cpp
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
    siligen_workflow_contracts_public
    siligen_workflow_domain_public
    siligen_workflow_application_public
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

siligen_add_test_with_runtime_environment(
    siligen_dispensing_semantics_tests
    siligen_dispensing_semantics_tests
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/motion/DeterministicPathExecutionUseCaseTest.cpp")
    add_executable(workflow_deterministic_path_execution_test
        motion/DeterministicPathExecutionUseCaseTest.cpp
    )
    target_include_directories(workflow_deterministic_path_execution_test PRIVATE
        ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
    )
    target_link_libraries(workflow_deterministic_path_execution_test PRIVATE
        siligen_application_motion
        siligen_types
        siligen_utils
    )
    target_compile_features(workflow_deterministic_path_execution_test PRIVATE cxx_std_17)
    siligen_stage_runtime_dependency(workflow_deterministic_path_execution_test ruckig)
    siligen_add_test_with_runtime_environment(
        workflow_deterministic_path_execution_test
        workflow_deterministic_path_execution_test
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/adapters/planning/dxf/PbPathSourceAdapterContractTest.cpp")
    add_executable(workflow_pb_path_source_adapter_test
        infrastructure/adapters/planning/dxf/PbPathSourceAdapterContractTest.cpp
    )
    target_include_directories(workflow_pb_path_source_adapter_test PRIVATE
        ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
    )
    target_link_libraries(workflow_pb_path_source_adapter_test PRIVATE
        siligen_parsing_adapter
        siligen_types
        siligen_utils
    )
    target_compile_features(workflow_pb_path_source_adapter_test PRIVATE cxx_std_17)
    siligen_add_test_with_runtime_environment(
        PbPathSourceAdapterContractTest
        workflow_pb_path_source_adapter_test
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/adapters/planning/dxf/DXFAdapterFactoryTest.cpp")
    add_executable(workflow_dxf_adapter_factory_test
        infrastructure/adapters/planning/dxf/DXFAdapterFactoryTest.cpp
    )
    target_include_directories(workflow_dxf_adapter_factory_test PRIVATE
        ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
    )
    if(TARGET GTest::gtest_main)
        target_link_libraries(workflow_dxf_adapter_factory_test PRIVATE GTest::gtest_main)
    elseif(TARGET gtest_main)
        target_link_libraries(workflow_dxf_adapter_factory_test PRIVATE gtest_main)
    else()
        message(FATAL_ERROR "GoogleTest target not found")
    endif()
    target_link_libraries(workflow_dxf_adapter_factory_test PRIVATE
        siligen_parsing_adapter
        siligen_types
        siligen_utils
    )
    target_compile_features(workflow_dxf_adapter_factory_test PRIVATE cxx_std_17)
    siligen_add_test_with_runtime_environment(
        DXFAdapterFactoryTest
        workflow_dxf_adapter_factory_test
    )
endif()

if(NOT TARGET workflow_unit_tests)
    add_custom_target(workflow_unit_tests
        DEPENDS
            siligen_unit_tests
            siligen_pr1_tests
            siligen_dispensing_semantics_tests
    )
endif()

if(TARGET workflow_unit_tests)
    if(TARGET workflow_deterministic_path_execution_test)
        add_dependencies(workflow_unit_tests
            workflow_deterministic_path_execution_test
        )
    endif()
    if(TARGET workflow_pb_path_source_adapter_test)
        add_dependencies(workflow_unit_tests
            workflow_pb_path_source_adapter_test
        )
    endif()
    if(TARGET workflow_dxf_adapter_factory_test)
        add_dependencies(workflow_unit_tests
            workflow_dxf_adapter_factory_test
        )
    endif()
endif()
