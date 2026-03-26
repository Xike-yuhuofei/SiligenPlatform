cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_application_motion)
    message(FATAL_ERROR "workflow integration tests 需要先加载 siligen_application_motion target")
endif()

set(WORKFLOW_INTEGRATION_ASSEMBLY_TEST_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/../process-runtime-core/unit/motion/MotionRuntimeAssemblyFactoryTest.cpp"
)

if(EXISTS "${WORKFLOW_INTEGRATION_ASSEMBLY_TEST_SOURCE}")
    add_executable(workflow_integration_motion_runtime_assembly_smoke
        "${WORKFLOW_INTEGRATION_ASSEMBLY_TEST_SOURCE}"
    )

    target_include_directories(workflow_integration_motion_runtime_assembly_smoke PRIVATE
        ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
    )
    target_link_libraries(workflow_integration_motion_runtime_assembly_smoke PRIVATE
        siligen_application_motion
        siligen_types
        siligen_utils
    )
    target_compile_features(workflow_integration_motion_runtime_assembly_smoke PRIVATE cxx_std_17)

    process_runtime_core_stage_runtime_dependency(
        workflow_integration_motion_runtime_assembly_smoke
        ruckig
    )
    process_runtime_core_add_test_with_runtime_environment(
        workflow_integration_motion_runtime_assembly_smoke
        workflow_integration_motion_runtime_assembly_smoke
    )
endif()

if(NOT TARGET workflow_integration_tests)
    add_custom_target(workflow_integration_tests)
endif()

if(TARGET workflow_integration_motion_runtime_assembly_smoke)
    add_dependencies(workflow_integration_tests
        workflow_integration_motion_runtime_assembly_smoke
    )
endif()
