cmake_minimum_required(VERSION 3.20)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/domain" "${CMAKE_BINARY_DIR}/modules/recipe-lifecycle/domain")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/application" "${CMAKE_BINARY_DIR}/modules/recipe-lifecycle/application")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/adapters" "${CMAKE_BINARY_DIR}/modules/recipe-lifecycle/adapters")

if(NOT TARGET siligen_recipe_lifecycle_domain_public)
    add_library(siligen_recipe_lifecycle_domain_public INTERFACE)
endif()
target_link_libraries(siligen_recipe_lifecycle_domain_public INTERFACE
    siligen_recipe_lifecycle_domain_headers
)
target_compile_features(siligen_recipe_lifecycle_domain_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_recipe_lifecycle_application_public)
    add_library(siligen_recipe_lifecycle_application_public INTERFACE)
endif()
target_link_libraries(siligen_recipe_lifecycle_application_public INTERFACE
    siligen_recipe_lifecycle_domain_public
    siligen_recipe_lifecycle_application_headers
    $<LINK_ONLY:siligen_recipe_lifecycle_application>
)
target_compile_features(siligen_recipe_lifecycle_application_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_recipe_lifecycle_serialization_public)
    add_library(siligen_recipe_lifecycle_serialization_public INTERFACE)
endif()
target_link_libraries(siligen_recipe_lifecycle_serialization_public INTERFACE
    siligen_recipe_lifecycle_domain_public
    siligen_recipe_lifecycle_adapters_headers
    $<LINK_ONLY:siligen_recipe_lifecycle_serialization>
)
target_compile_features(siligen_recipe_lifecycle_serialization_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_module_recipe_lifecycle)
    add_library(siligen_module_recipe_lifecycle INTERFACE)
endif()
target_link_libraries(siligen_module_recipe_lifecycle INTERFACE
    siligen_recipe_lifecycle_application_public
    siligen_recipe_lifecycle_serialization_public
)
target_compile_features(siligen_module_recipe_lifecycle INTERFACE cxx_std_17)
set_target_properties(siligen_module_recipe_lifecycle PROPERTIES
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/recipe-lifecycle"
)

if(
    SILIGEN_BUILD_TESTS AND
    (TARGET GTest::gtest_main OR TARGET gtest_main) AND
    EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt"
)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/tests" "${CMAKE_BINARY_DIR}/modules/recipe-lifecycle/tests")
endif()
