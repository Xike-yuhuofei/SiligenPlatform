cmake_minimum_required(VERSION 3.20)

set(SILIGEN_RECIPE_LIFECYCLE_ADAPTERS_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_RECIPE_LIFECYCLE_ADAPTERS_PUBLIC_INCLUDE_DIR}/recipe_lifecycle/adapters/serialization")
    message(FATAL_ERROR
        "recipe-lifecycle canonical adapters include root missing: "
        "${SILIGEN_RECIPE_LIFECYCLE_ADAPTERS_PUBLIC_INCLUDE_DIR}/recipe_lifecycle/adapters/serialization"
    )
endif()

if(NOT TARGET siligen_recipe_lifecycle_adapters_headers)
    add_library(siligen_recipe_lifecycle_adapters_headers INTERFACE)
endif()
target_include_directories(siligen_recipe_lifecycle_adapters_headers INTERFACE
    ${SILIGEN_RECIPE_LIFECYCLE_ADAPTERS_PUBLIC_INCLUDE_DIR}
)
target_link_libraries(siligen_recipe_lifecycle_adapters_headers INTERFACE
    siligen_recipe_lifecycle_domain_headers
    siligen_workspace_json_headers
)
target_compile_features(siligen_recipe_lifecycle_adapters_headers INTERFACE cxx_std_17)

add_library(siligen_recipe_lifecycle_serialization STATIC
    serialization/RecipeJsonSerializer.cpp
)
target_include_directories(siligen_recipe_lifecycle_serialization PUBLIC
    ${SILIGEN_RECIPE_LIFECYCLE_ADAPTERS_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_recipe_lifecycle_serialization PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(siligen_recipe_lifecycle_serialization PUBLIC
    siligen_recipe_lifecycle_adapters_headers
    siligen_recipe_lifecycle_domain_headers
    siligen_workspace_json_headers
    siligen_types
    siligen_utils
)
target_compile_features(siligen_recipe_lifecycle_serialization PUBLIC cxx_std_17)
