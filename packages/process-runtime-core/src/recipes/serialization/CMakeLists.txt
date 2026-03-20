add_library(siligen_recipe_json_codec STATIC
    RecipeJsonSerializer.cpp
)

target_include_directories(siligen_recipe_json_codec PUBLIC
    ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
    ${SILIGEN_THIRD_PARTY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(siligen_recipe_json_codec PUBLIC
    siligen_domain
    siligen_types
    siligen_utils
)

target_compile_features(siligen_recipe_json_codec PUBLIC cxx_std_17)
