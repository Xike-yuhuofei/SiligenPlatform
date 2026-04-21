cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_motion_planning_application_public AND NOT TARGET siligen_motion_planning_application)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../motion-planning/application"
        "${CMAKE_BINARY_DIR}/_p/dpt_mpa"
    )
endif()

set(SILIGEN_DISPENSE_PACKAGING_TEST_MOTION_PLANNING_APP_TARGET siligen_motion_planning_application_public)
if(NOT TARGET ${SILIGEN_DISPENSE_PACKAGING_TEST_MOTION_PLANNING_APP_TARGET} AND TARGET siligen_motion_planning_application)
    set(SILIGEN_DISPENSE_PACKAGING_TEST_MOTION_PLANNING_APP_TARGET siligen_motion_planning_application)
endif()

set(DISPENSE_PACKAGING_UNIT_TEST_SOURCES
    unit/domain/dispensing/ArcTriggerPointCalculatorTest.cpp
    unit/domain/dispensing/TriggerPlannerTest.cpp
    unit/domain/dispensing/PathArcLengthLocatorTest.cpp
    unit/application/services/dispensing/PreviewSnapshotServiceTest.cpp
    unit/application/usecases/dispensing/PlanningRequestTest.cpp
    unit/application/usecases/dispensing/PlanningUseCaseExportPortTest.cpp
    unit/application/usecases/dispensing/PlanningFailureSurfaceTest.cpp
    unit/application/usecases/dispensing/valve/ValveUseCasesTest.cpp
)

set(DISPENSE_PACKAGING_OPTIONAL_TEST_SOURCES
    unit/domain/dispensing/AuthorityTriggerLayoutPlannerTest.cpp
)

set(DISPENSE_PACKAGING_RESIDUAL_REGRESSION_TEST_SOURCES
    regression/domain/dispensing/DispensePackagingResidualAcceptanceTest.cpp
)

set(DISPENSE_PACKAGING_PLANNING_RESIDUAL_REGRESSION_TEST_SOURCES
    regression/domain/dispensing/planning/ContourPathOptimizerTest.cpp
    unit/domain/dispensing/LegacyDxfPlannerQuarantineAuditTest.cpp
)

set(DISPENSE_PACKAGING_EXECUTION_RESIDUAL_REGRESSION_TEST_SOURCES
    regression/domain/dispensing/execution/PurgeDispenserProcessTest.cpp
    regression/domain/dispensing/execution/ValveCoordinationServiceTest.cpp
    regression/domain/dispensing/execution/DispensingControllerTest.cpp
)

foreach(test_source IN LISTS DISPENSE_PACKAGING_OPTIONAL_TEST_SOURCES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test_source}")
        list(APPEND DISPENSE_PACKAGING_UNIT_TEST_SOURCES "${test_source}")
    endif()
endforeach()

add_executable(siligen_dispense_packaging_unit_tests ${DISPENSE_PACKAGING_UNIT_TEST_SOURCES})
add_library(
    siligen_dispense_packaging_planning_legacy_dxf_quarantine_support STATIC
    ../domain/dispensing/planning/domain-services/ContourOptimizationService.cpp
    ../domain/dispensing/planning/domain-services/DispensingPlannerService.cpp
)
add_executable(
    siligen_dispense_packaging_boundary_tests
    contract/domain/dispensing/DispensePackagingBoundaryTest.cpp
)
add_executable(
    siligen_dispense_packaging_workflow_seam_tests
    contract/application/services/dispensing/WorkflowPlanningAssemblyOperationsContractTest.cpp
)
add_executable(
    siligen_dispense_packaging_residual_regression_tests
    ${DISPENSE_PACKAGING_RESIDUAL_REGRESSION_TEST_SOURCES}
)
add_executable(
    siligen_dispense_packaging_planning_residual_regression_tests
    ${DISPENSE_PACKAGING_PLANNING_RESIDUAL_REGRESSION_TEST_SOURCES}
)
add_executable(
    siligen_dispense_packaging_execution_residual_regression_tests
    ${DISPENSE_PACKAGING_EXECUTION_RESIDUAL_REGRESSION_TEST_SOURCES}
)
add_executable(
    siligen_dispense_packaging_shared_adjacent_regression_tests
    regression/shared/types/TrajectoryTriggerUtilsTest.cpp
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_dispense_packaging_unit_tests PRIVATE GTest::gtest_main)
    target_link_libraries(siligen_dispense_packaging_boundary_tests PRIVATE GTest::gtest_main)
    target_link_libraries(siligen_dispense_packaging_workflow_seam_tests PRIVATE GTest::gtest_main)
    target_link_libraries(siligen_dispense_packaging_residual_regression_tests PRIVATE GTest::gtest_main)
    target_link_libraries(siligen_dispense_packaging_planning_residual_regression_tests PRIVATE GTest::gtest_main)
    target_link_libraries(siligen_dispense_packaging_execution_residual_regression_tests PRIVATE GTest::gtest_main)
    target_link_libraries(siligen_dispense_packaging_shared_adjacent_regression_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_dispense_packaging_unit_tests PRIVATE gtest_main)
    target_link_libraries(siligen_dispense_packaging_boundary_tests PRIVATE gtest_main)
    target_link_libraries(siligen_dispense_packaging_workflow_seam_tests PRIVATE gtest_main)
    target_link_libraries(siligen_dispense_packaging_residual_regression_tests PRIVATE gtest_main)
    target_link_libraries(siligen_dispense_packaging_planning_residual_regression_tests PRIVATE gtest_main)
    target_link_libraries(siligen_dispense_packaging_execution_residual_regression_tests PRIVATE gtest_main)
    target_link_libraries(siligen_dispense_packaging_shared_adjacent_regression_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_include_directories(siligen_dispense_packaging_planning_legacy_dxf_quarantine_support BEFORE PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../domain/dispensing
    ${CMAKE_CURRENT_SOURCE_DIR}/../domain
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_dispense_packaging_planning_legacy_dxf_quarantine_support PUBLIC
    siligen_dispense_packaging_domain_dispensing_planning_residual_headers
    siligen_dispense_packaging_planning_owner_residual
    siligen_types
    siligen_utils
    siligen_geometry
    siligen_motion
    siligen_process_path_application_public
    ${SILIGEN_DISPENSE_PACKAGING_TEST_MOTION_PLANNING_APP_TARGET}
)
target_link_libraries(siligen_dispense_packaging_planning_legacy_dxf_quarantine_support PRIVATE
    siligen_dispense_packaging_execution_residual
)
target_compile_features(siligen_dispense_packaging_planning_legacy_dxf_quarantine_support PUBLIC cxx_std_17)

target_link_libraries(siligen_dispense_packaging_unit_tests PRIVATE
    siligen_dispense_packaging_application_public
    siligen_dxf_geometry_application_public
    siligen_dxf_geometry_pb_path_source_adapter
    siligen_process_path_application_public
    ${SILIGEN_DISPENSE_PACKAGING_TEST_MOTION_PLANNING_APP_TARGET}
    siligen_dispense_packaging_planning_owner_residual
    siligen_dispense_packaging_execution_residual
    siligen_valve_core
    siligen_types
    siligen_utils
)
target_link_libraries(siligen_dispense_packaging_boundary_tests PRIVATE
    siligen_dispense_packaging_application_public
    siligen_types
    siligen_utils
)
target_link_libraries(siligen_dispense_packaging_workflow_seam_tests PRIVATE
    siligen_dispense_packaging_application_public
    siligen_types
    siligen_utils
)
target_link_libraries(siligen_dispense_packaging_residual_regression_tests PRIVATE
    siligen_dispense_packaging_application_public
    siligen_types
    siligen_utils
)
target_link_libraries(siligen_dispense_packaging_planning_residual_regression_tests PRIVATE
    siligen_dispense_packaging_planning_legacy_dxf_quarantine_support
    siligen_types
    siligen_utils
)
target_link_libraries(siligen_dispense_packaging_execution_residual_regression_tests PRIVATE
    siligen_dispense_packaging_execution_residual
    siligen_types
    siligen_utils
)
target_link_libraries(siligen_dispense_packaging_shared_adjacent_regression_tests PRIVATE
    siligen_types
    siligen_utils
)
target_compile_features(siligen_dispense_packaging_unit_tests PRIVATE cxx_std_17)
target_compile_features(siligen_dispense_packaging_boundary_tests PRIVATE cxx_std_17)
target_compile_features(siligen_dispense_packaging_workflow_seam_tests PRIVATE cxx_std_17)
target_compile_features(siligen_dispense_packaging_residual_regression_tests PRIVATE cxx_std_17)
target_compile_features(siligen_dispense_packaging_planning_residual_regression_tests PRIVATE cxx_std_17)
target_compile_features(siligen_dispense_packaging_execution_residual_regression_tests PRIVATE cxx_std_17)
target_compile_features(siligen_dispense_packaging_shared_adjacent_regression_tests PRIVATE cxx_std_17)

if(COMMAND siligen_add_test_with_runtime_environment)
    siligen_add_test_with_runtime_environment(
        siligen_dispense_packaging_unit_tests
        siligen_dispense_packaging_unit_tests
    )
    siligen_add_test_with_runtime_environment(
        siligen_dispense_packaging_boundary_tests
        siligen_dispense_packaging_boundary_tests
    )
    siligen_add_test_with_runtime_environment(
        siligen_dispense_packaging_workflow_seam_tests
        siligen_dispense_packaging_workflow_seam_tests
    )
    siligen_add_test_with_runtime_environment(
        siligen_dispense_packaging_residual_regression_tests
        siligen_dispense_packaging_residual_regression_tests
    )
    siligen_add_test_with_runtime_environment(
        siligen_dispense_packaging_planning_residual_regression_tests
        siligen_dispense_packaging_planning_residual_regression_tests
    )
    siligen_add_test_with_runtime_environment(
        siligen_dispense_packaging_execution_residual_regression_tests
        siligen_dispense_packaging_execution_residual_regression_tests
    )
    siligen_add_test_with_runtime_environment(
        siligen_dispense_packaging_shared_adjacent_regression_tests
        siligen_dispense_packaging_shared_adjacent_regression_tests
    )
else()
    add_test(NAME siligen_dispense_packaging_unit_tests COMMAND siligen_dispense_packaging_unit_tests)
    add_test(NAME siligen_dispense_packaging_boundary_tests COMMAND siligen_dispense_packaging_boundary_tests)
    add_test(NAME siligen_dispense_packaging_workflow_seam_tests COMMAND siligen_dispense_packaging_workflow_seam_tests)
    add_test(
        NAME siligen_dispense_packaging_residual_regression_tests
        COMMAND siligen_dispense_packaging_residual_regression_tests
    )
    add_test(
        NAME siligen_dispense_packaging_planning_residual_regression_tests
        COMMAND siligen_dispense_packaging_planning_residual_regression_tests
    )
    add_test(
        NAME siligen_dispense_packaging_execution_residual_regression_tests
        COMMAND siligen_dispense_packaging_execution_residual_regression_tests
    )
    add_test(
        NAME siligen_dispense_packaging_shared_adjacent_regression_tests
        COMMAND siligen_dispense_packaging_shared_adjacent_regression_tests
    )
endif()
