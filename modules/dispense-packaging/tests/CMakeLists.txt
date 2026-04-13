cmake_minimum_required(VERSION 3.20)

set(DISPENSE_PACKAGING_TEST_SOURCES
    unit/domain/dispensing/ArcTriggerPointCalculatorTest.cpp
    unit/domain/dispensing/ContourPathOptimizerTest.cpp
    unit/domain/dispensing/TriggerPlannerTest.cpp
    unit/domain/dispensing/DispensingControllerTest.cpp
    unit/domain/dispensing/DispensePackagingResidualAcceptanceTest.cpp
    unit/domain/dispensing/PathArcLengthLocatorTest.cpp
    unit/domain/dispensing/PurgeDispenserProcessTest.cpp
    unit/domain/dispensing/UnifiedTrajectoryPlannerServiceTest.cpp
    unit/domain/dispensing/TrajectoryTriggerUtilsTest.cpp
    unit/domain/dispensing/ValveCoordinationServiceTest.cpp
    unit/application/services/dispensing/PreviewSnapshotServiceTest.cpp
    unit/application/usecases/dispensing/valve/ValveUseCasesTest.cpp
)

set(DISPENSE_PACKAGING_OPTIONAL_TEST_SOURCES
    unit/domain/dispensing/AuthorityTriggerLayoutPlannerTest.cpp
)

foreach(test_source IN LISTS DISPENSE_PACKAGING_OPTIONAL_TEST_SOURCES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test_source}")
        list(APPEND DISPENSE_PACKAGING_TEST_SOURCES "${test_source}")
    endif()
endforeach()

add_executable(siligen_dispense_packaging_unit_tests ${DISPENSE_PACKAGING_TEST_SOURCES})
add_executable(
    siligen_dispense_packaging_boundary_tests
    unit/domain/dispensing/DispensePackagingBoundaryTest.cpp
)
add_executable(
    siligen_dispense_packaging_workflow_seam_tests
    unit/application/services/dispensing/WorkflowPlanningAssemblyOperationsContractTest.cpp
)
add_executable(
    siligen_dispense_packaging_planning_tests
    unit/application/usecases/dispensing/PlanningRequestTest.cpp
    unit/application/usecases/dispensing/PlanningUseCaseExportPortTest.cpp
    unit/application/usecases/dispensing/PlanningFailureSurfaceTest.cpp
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_dispense_packaging_unit_tests PRIVATE GTest::gtest_main)
    target_link_libraries(siligen_dispense_packaging_boundary_tests PRIVATE GTest::gtest_main)
    target_link_libraries(siligen_dispense_packaging_workflow_seam_tests PRIVATE GTest::gtest_main)
    target_link_libraries(siligen_dispense_packaging_planning_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_dispense_packaging_unit_tests PRIVATE gtest_main)
    target_link_libraries(siligen_dispense_packaging_boundary_tests PRIVATE gtest_main)
    target_link_libraries(siligen_dispense_packaging_workflow_seam_tests PRIVATE gtest_main)
    target_link_libraries(siligen_dispense_packaging_planning_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_dispense_packaging_unit_tests PRIVATE
    siligen_dispense_packaging_application_public
    siligen_dispense_packaging_planning_residual
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
target_link_libraries(siligen_dispense_packaging_planning_tests PRIVATE
    siligen_dispense_packaging_application_public
    siligen_process_planning_contracts_public
    siligen_process_path_application_public
    siligen_motion_planning_application_public
    siligen_dxf_geometry_application_public
    siligen_trace_diagnostics_contracts_public
    siligen_shared_runtime_contracts
    siligen_types
    siligen_utils
)
target_compile_features(siligen_dispense_packaging_unit_tests PRIVATE cxx_std_17)
target_compile_features(siligen_dispense_packaging_boundary_tests PRIVATE cxx_std_17)
target_compile_features(siligen_dispense_packaging_workflow_seam_tests PRIVATE cxx_std_17)
target_compile_features(siligen_dispense_packaging_planning_tests PRIVATE cxx_std_17)

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
        siligen_dispense_packaging_planning_tests
        siligen_dispense_packaging_planning_tests
    )
else()
    add_test(NAME siligen_dispense_packaging_unit_tests COMMAND siligen_dispense_packaging_unit_tests)
    add_test(NAME siligen_dispense_packaging_boundary_tests COMMAND siligen_dispense_packaging_boundary_tests)
    add_test(NAME siligen_dispense_packaging_workflow_seam_tests COMMAND siligen_dispense_packaging_workflow_seam_tests)
    add_test(NAME siligen_dispense_packaging_planning_tests COMMAND siligen_dispense_packaging_planning_tests)
endif()
