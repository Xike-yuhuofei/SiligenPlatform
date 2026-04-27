cmake_minimum_required(VERSION 3.20)

set(MOTION_PLANNING_UNIT_TEST_SOURCES
    unit/domain/trajectory/MotionPlannerTest.cpp
    unit/domain/trajectory/MotionPlannerConstraintTest.cpp
    unit/domain/trajectory/InterpolationProgramPlannerTest.cpp
    unit/domain/motion/CMPCoordinatedInterpolatorPrecisionTest.cpp
    unit/domain/motion/MotionPlannerOwnerPathTest.cpp
    unit/domain/motion/SCurveProfileMathTest.cpp
    unit/application/usecases/motion/trajectory/DeterministicPathExecutionUseCaseTest.cpp
    unit/application/services/motion_planning/MotionPlanningPublicSurfaceContractTest.cpp
)

set(MOTION_PLANNING_ARCHITECTURE_TEST_SOURCES
    unit/domain/motion/MotionExecutionOwnerBoundaryTest.cpp
    unit/domain/motion/MotionPlanningOwnerBoundaryTest.cpp
    unit/domain/motion/NoRuntimeControlLeakTest.cpp
)

set(MOTION_PLANNING_INTEGRATION_TEST_SOURCES
    unit/domain/motion/MainlineTrajectoryAuditTest.cpp
    unit/application/services/motion_planning/MotionPlanningFacadeTest.cpp
)

if(TARGET GTest::gtest_main)
    set(SILIGEN_MOTION_PLANNING_GTEST_TARGET GTest::gtest_main)
elseif(TARGET gtest_main)
    set(SILIGEN_MOTION_PLANNING_GTEST_TARGET gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

function(siligen_motion_planning_register_test target)
    target_link_libraries(${target} PRIVATE ${SILIGEN_MOTION_PLANNING_GTEST_TARGET})
    target_compile_features(${target} PRIVATE cxx_std_17)

    if(COMMAND siligen_add_test_with_runtime_environment)
        siligen_add_test_with_runtime_environment(${target} ${target})
    else()
        add_test(NAME ${target} COMMAND ${target})
    endif()
endfunction()

add_executable(siligen_motion_planning_unit_tests ${MOTION_PLANNING_UNIT_TEST_SOURCES})
target_include_directories(siligen_motion_planning_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(siligen_motion_planning_unit_tests PRIVATE
    siligen_motion_planning_application_public
    siligen_motion
    siligen_runtime_execution_runtime_contracts
    siligen_types
    siligen_utils
)
siligen_motion_planning_register_test(siligen_motion_planning_unit_tests)

add_executable(siligen_motion_planning_architecture_tests ${MOTION_PLANNING_ARCHITECTURE_TEST_SOURCES})
target_include_directories(siligen_motion_planning_architecture_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(siligen_motion_planning_architecture_tests PRIVATE
    siligen_motion_planning_application_public
    siligen_runtime_execution_application_public
    siligen_runtime_execution_runtime_contracts
    siligen_motion
    siligen_types
    siligen_utils
)
siligen_motion_planning_register_test(siligen_motion_planning_architecture_tests)

add_executable(siligen_motion_planning_integration_tests ${MOTION_PLANNING_INTEGRATION_TEST_SOURCES})
target_include_directories(siligen_motion_planning_integration_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(siligen_motion_planning_integration_tests PRIVATE
    siligen_motion_planning_application_public
    siligen_motion
    siligen_runtime_execution_runtime_contracts
    siligen_module_dxf_geometry
    siligen_types
    siligen_utils
)
if(TARGET siligen_process_path_application_public)
    target_link_libraries(siligen_motion_planning_integration_tests PRIVATE
        siligen_process_path_application_public
    )
endif()
siligen_motion_planning_register_test(siligen_motion_planning_integration_tests)
