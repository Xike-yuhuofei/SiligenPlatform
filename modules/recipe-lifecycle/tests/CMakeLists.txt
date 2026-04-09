cmake_minimum_required(VERSION 3.20)

add_executable(siligen_recipe_lifecycle_unit_tests
    canonical/unit/domain/recipes/RecipeActivationServiceTest.cpp
    canonical/unit/recipes/RecipeCommandUseCaseTest.cpp
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_recipe_lifecycle_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_recipe_lifecycle_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_recipe_lifecycle_unit_tests PRIVATE
    siligen_recipe_lifecycle_application_public
    siligen_recipe_lifecycle_domain_services
    siligen_types
    siligen_utils
)
target_compile_features(siligen_recipe_lifecycle_unit_tests PRIVATE cxx_std_17)
target_compile_definitions(siligen_recipe_lifecycle_unit_tests PRIVATE SILIGEN_TEST_HOOKS=1)
add_test(NAME siligen_recipe_lifecycle_unit_tests COMMAND siligen_recipe_lifecycle_unit_tests)
