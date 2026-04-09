cmake_minimum_required(VERSION 3.20)

set(SILIGEN_RECIPE_LIFECYCLE_APPLICATION_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_RECIPE_LIFECYCLE_APPLICATION_PUBLIC_INCLUDE_DIR}/recipe_lifecycle/application/usecases/recipes")
    message(FATAL_ERROR
        "recipe-lifecycle canonical application include root missing: "
        "${SILIGEN_RECIPE_LIFECYCLE_APPLICATION_PUBLIC_INCLUDE_DIR}/recipe_lifecycle/application/usecases/recipes"
    )
endif()

if(NOT TARGET siligen_recipe_lifecycle_application_headers)
    add_library(siligen_recipe_lifecycle_application_headers INTERFACE)
endif()
target_include_directories(siligen_recipe_lifecycle_application_headers INTERFACE
    ${SILIGEN_RECIPE_LIFECYCLE_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_link_libraries(siligen_recipe_lifecycle_application_headers INTERFACE
    siligen_recipe_lifecycle_domain_headers
    siligen_types
    siligen_utils
)
target_compile_features(siligen_recipe_lifecycle_application_headers INTERFACE cxx_std_17)

add_library(siligen_recipe_lifecycle_application STATIC
    usecases/recipes/CompareRecipeVersionsUseCase.cpp
    usecases/recipes/CreateDraftVersionUseCase.cpp
    usecases/recipes/CreateRecipeUseCase.cpp
    usecases/recipes/CreateVersionFromPublishedUseCase.cpp
    usecases/recipes/ExportRecipeBundlePayloadUseCase.cpp
    usecases/recipes/ImportRecipeBundlePayloadUseCase.cpp
    usecases/recipes/RecipeCommandUseCase.cpp
    usecases/recipes/RecipeQueryUseCase.cpp
    usecases/recipes/UpdateDraftVersionUseCase.cpp
    usecases/recipes/UpdateRecipeUseCase.cpp
)
target_include_directories(siligen_recipe_lifecycle_application PUBLIC
    ${SILIGEN_RECIPE_LIFECYCLE_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_recipe_lifecycle_application PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(siligen_recipe_lifecycle_application PUBLIC
    siligen_recipe_lifecycle_application_headers
    siligen_recipe_lifecycle_domain_services
    siligen_types
    siligen_utils
)
target_compile_features(siligen_recipe_lifecycle_application PUBLIC cxx_std_17)
