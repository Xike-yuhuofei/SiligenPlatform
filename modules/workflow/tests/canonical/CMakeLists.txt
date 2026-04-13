cmake_minimum_required(VERSION 3.20)

foreach(required_target
    IN ITEMS
        siligen_module_workflow
        siligen_workflow_contracts_public
        siligen_workflow_domain_public
        siligen_types
        siligen_utils
)
    if(NOT TARGET ${required_target})
        message(FATAL_ERROR "workflow canonical tests 缺少必需 target: ${required_target}")
    endif()
endforeach()
set(WORKFLOW_CANONICAL_FORBIDDEN_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/dispensing/PlanningRequestTest.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/redundancy/CandidateScoringServiceTest.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/redundancy/RedundancyGovernanceUseCasesTest.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapterTest.cpp"
)
foreach(forbidden_source IN LISTS WORKFLOW_CANONICAL_FORBIDDEN_SOURCES)
    if(EXISTS "${forbidden_source}")
        message(FATAL_ERROR "workflow canonical tests must remain M0-only: ${forbidden_source}")
    endif()
endforeach()

set(WORKFLOW_CANONICAL_UNIT_TEST_SOURCES
    unit/domain/workflow/WorkflowDomainModelTest.cpp
    unit/domain/workflow/WorkflowSkeletonSurfaceTest.cpp
)

set(WORKFLOW_CANONICAL_SKELETON_INCLUDE_DIRS
    "${WORKFLOW_MODULE_DIR}"
    "${WORKFLOW_MODULE_DIR}/application/include"
    "${WORKFLOW_MODULE_DIR}/runtime"
)

add_executable(siligen_unit_tests ${WORKFLOW_CANONICAL_UNIT_TEST_SOURCES})

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_unit_tests PRIVATE
    siligen_module_workflow
    siligen_workflow_contracts_public
    siligen_workflow_domain_public
    siligen_types
    siligen_utils
)
target_compile_definitions(siligen_unit_tests PRIVATE SILIGEN_TEST_HOOKS=1)
target_include_directories(siligen_unit_tests PRIVATE
    ${WORKFLOW_TEST_IMPLEMENTATION_INCLUDE_DIRS}
    ${WORKFLOW_CANONICAL_SKELETON_INCLUDE_DIRS}
)

siligen_add_test_with_runtime_environment(
    siligen_unit_tests
    siligen_unit_tests
)

if(NOT TARGET workflow_canonical_tests)
    add_custom_target(workflow_canonical_tests
        DEPENDS
            siligen_unit_tests
    )
endif()
