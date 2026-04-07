cmake_minimum_required(VERSION 3.20)

set(MOTION_PLANNING_TEST_SOURCES
    unit/domain/trajectory/MotionPlannerTest.cpp
    unit/domain/trajectory/MotionPlannerConstraintTest.cpp
    unit/domain/trajectory/InterpolationProgramPlannerTest.cpp
    unit/domain/motion/CMPCoordinatedInterpolatorPrecisionTest.cpp
    unit/domain/motion/MainlineTrajectoryAuditTest.cpp
    unit/domain/motion/MotionExecutionOwnerBoundaryTest.cpp
    unit/domain/motion/MotionPlanningOwnerBoundaryTest.cpp
    unit/domain/motion/NoRuntimeControlLeakTest.cpp
    unit/domain/motion/MotionPlannerOwnerPathTest.cpp
    unit/application/services/motion_planning/MotionPlanningFacadeTest.cpp
    unit/application/services/motion_planning/MotionPlanningPublicSurfaceContractTest.cpp
)

add_executable(siligen_motion_planning_unit_tests ${MOTION_PLANNING_TEST_SOURCES})

target_include_directories(siligen_motion_planning_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/modules/workflow/domain/include
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_motion_planning_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_motion_planning_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_motion_planning_unit_tests PRIVATE
    siligen_motion_planning_application_public
    siligen_motion
    siligen_dispense_packaging_application_public
    siligen_runtime_execution_runtime_contracts
    siligen_parsing_adapter
    siligen_types
    siligen_utils
)
target_compile_features(siligen_motion_planning_unit_tests PRIVATE cxx_std_17)

if(COMMAND siligen_add_test_with_runtime_environment)
    siligen_add_test_with_runtime_environment(
        siligen_motion_planning_unit_tests
        siligen_motion_planning_unit_tests
    )
else()
    add_test(NAME siligen_motion_planning_unit_tests COMMAND siligen_motion_planning_unit_tests)
endif()
