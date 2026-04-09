cmake_minimum_required(VERSION 3.20)

set(SILIGEN_RECIPE_LIFECYCLE_DOMAIN_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_RECIPE_LIFECYCLE_DOMAIN_PUBLIC_INCLUDE_DIR}/recipe_lifecycle/domain/recipes")
    message(FATAL_ERROR
        "recipe-lifecycle canonical domain include root missing: "
        "${SILIGEN_RECIPE_LIFECYCLE_DOMAIN_PUBLIC_INCLUDE_DIR}/recipe_lifecycle/domain/recipes"
    )
endif()

if(NOT TARGET siligen_recipe_lifecycle_domain_headers)
    add_library(siligen_recipe_lifecycle_domain_headers INTERFACE)
endif()
target_include_directories(siligen_recipe_lifecycle_domain_headers INTERFACE
    ${SILIGEN_RECIPE_LIFECYCLE_DOMAIN_PUBLIC_INCLUDE_DIR}
)
target_link_libraries(siligen_recipe_lifecycle_domain_headers INTERFACE
    siligen_types
    siligen_utils
)
target_compile_features(siligen_recipe_lifecycle_domain_headers INTERFACE cxx_std_17)

add_library(siligen_recipe_lifecycle_domain_services STATIC
    recipes/domain-services/RecipeActivationService.cpp
    recipes/domain-services/RecipeValidationService.cpp
)
target_include_directories(siligen_recipe_lifecycle_domain_services PUBLIC
    ${SILIGEN_RECIPE_LIFECYCLE_DOMAIN_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_recipe_lifecycle_domain_services PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(siligen_recipe_lifecycle_domain_services PUBLIC
    siligen_recipe_lifecycle_domain_headers
    siligen_types
    siligen_utils
)
target_compile_features(siligen_recipe_lifecycle_domain_services PUBLIC cxx_std_17)
