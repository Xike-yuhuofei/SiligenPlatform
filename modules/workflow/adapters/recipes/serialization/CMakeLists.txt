add_library(siligen_recipe_json_codec STATIC
    RecipeJsonSerializer.cpp
)

target_include_directories(siligen_recipe_json_codec PUBLIC
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)
target_include_directories(siligen_recipe_json_codec PRIVATE
    ${PROCESS_RUNTIME_CORE_IMPLEMENTATION_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(siligen_recipe_json_codec PUBLIC
    siligen_domain
    siligen_process_runtime_core_json_headers
    siligen_types
    siligen_utils
)

target_compile_features(siligen_recipe_json_codec PUBLIC cxx_std_17)
