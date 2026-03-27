cmake_minimum_required(VERSION 3.20)

set(MOTION_PLANNING_TEST_SOURCES
    unit/domain/trajectory/MotionPlannerTest.cpp
    unit/domain/trajectory/MotionPlannerConstraintTest.cpp
    unit/domain/trajectory/InterpolationProgramPlannerTest.cpp
    unit/domain/motion/CMPCoordinatedInterpolatorPrecisionTest.cpp
    unit/domain/motion/NoRuntimeControlLeakTest.cpp
    unit/domain/motion/MotionPlannerOwnerPathTest.cpp
    unit/application/services/motion_planning/MotionPlanningFacadeTest.cpp
)

add_executable(siligen_motion_planning_unit_tests ${MOTION_PLANNING_TEST_SOURCES})

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_motion_planning_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_motion_planning_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_motion_planning_unit_tests PRIVATE
    siligen_motion_planning_application_public
    siligen_runtime_execution_runtime_contracts
    siligen_types
    siligen_utils
)
target_compile_features(siligen_motion_planning_unit_tests PRIVATE cxx_std_17)

if(COMMAND process_runtime_core_add_test_with_runtime_environment)
    process_runtime_core_add_test_with_runtime_environment(
        siligen_motion_planning_unit_tests
        siligen_motion_planning_unit_tests
    )
else()
    add_test(NAME siligen_motion_planning_unit_tests COMMAND siligen_motion_planning_unit_tests)
endif()
