cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_workflow_application_public)
    message(FATAL_ERROR "workflow canonical tests 需要先加载 siligen_workflow_application_public target")
endif()

set(WORKFLOW_CANONICAL_UNIT_TEST_SOURCES
    unit/domain/safety/InterlockPolicyTest.cpp
    unit/domain/safety/SoftLimitValidatorTest.cpp
    unit/domain/safety/SafetyOutputGuardTest.cpp
    unit/domain/recipes/RecipeActivationServiceTest.cpp
    unit/recipes/RecipeCommandUseCaseTest.cpp
    unit/dispensing/PlanningRequestTest.cpp
    unit/infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapterTest.cpp
    unit/redundancy/CandidateScoringServiceTest.cpp
    unit/redundancy/RedundancyGovernanceUseCasesTest.cpp
)

set(WORKFLOW_DOMAIN_SAFETY_TEST_SOURCES
    unit/domain/safety/InterlockPolicyTest.cpp
    unit/domain/safety/SoftLimitValidatorTest.cpp
    unit/domain/safety/SafetyOutputGuardTest.cpp
)
set(WORKFLOW_DOMAIN_RECIPE_TEST_SOURCES
    unit/domain/recipes/RecipeActivationServiceTest.cpp
)
set(WORKFLOW_APPLICATION_PLANNING_TRIGGER_TEST_SOURCES
    unit/dispensing/PlanningRequestTest.cpp
)
set(WORKFLOW_APPLICATION_COMPAT_TEST_SOURCES
    unit/recipes/RecipeCommandUseCaseTest.cpp
)
set(WORKFLOW_APPLICATION_RECOVERY_CONTROL_TEST_SOURCES
    unit/redundancy/CandidateScoringServiceTest.cpp
    unit/redundancy/RedundancyGovernanceUseCasesTest.cpp
)
set(WORKFLOW_ADAPTERS_STATE_TEST_SOURCES
    unit/infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapterTest.cpp
)

source_group("unit/domain/safety" FILES ${WORKFLOW_DOMAIN_SAFETY_TEST_SOURCES})
source_group("unit/domain/recipes" FILES ${WORKFLOW_DOMAIN_RECIPE_TEST_SOURCES})
source_group("unit/application/planning-trigger" FILES ${WORKFLOW_APPLICATION_PLANNING_TRIGGER_TEST_SOURCES})
source_group("unit/application/recovery-control" FILES ${WORKFLOW_APPLICATION_RECOVERY_CONTROL_TEST_SOURCES})
source_group("unit/application/compat" FILES ${WORKFLOW_APPLICATION_COMPAT_TEST_SOURCES})
source_group("unit/adapters/state" FILES ${WORKFLOW_ADAPTERS_STATE_TEST_SOURCES})

add_executable(siligen_unit_tests ${WORKFLOW_CANONICAL_UNIT_TEST_SOURCES})

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_unit_tests PRIVATE
    siligen_workflow_application_public
    siligen_redundancy_storage_adapter
    siligen_types
    siligen_utils
)
if(TARGET siligen_workflow_recipe_application_public)
    target_link_libraries(siligen_unit_tests PRIVATE
        siligen_workflow_recipe_application_public
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

set(WORKFLOW_PR1_TEST_SOURCES
    unit/domain/safety/InterlockPolicyTest.cpp
    unit/domain/safety/SafetyOutputGuardTest.cpp
    unit/dispensing/PlanningRequestTest.cpp
)

add_executable(siligen_pr1_tests ${WORKFLOW_PR1_TEST_SOURCES})

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_pr1_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_pr1_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_pr1_tests PRIVATE
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

if(NOT TARGET workflow_canonical_tests)
    add_custom_target(workflow_canonical_tests
        DEPENDS
            siligen_unit_tests
            siligen_pr1_tests
    )
endif()
