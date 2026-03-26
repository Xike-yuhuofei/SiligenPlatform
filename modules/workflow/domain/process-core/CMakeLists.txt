cmake_minimum_required(VERSION 3.20)

add_library(siligen_process_core STATIC
    src/recipes/services/recipe_validation_service.cpp
    src/recipes/services/recipe_activation_service.cpp
)

target_include_directories(siligen_process_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(siligen_process_core PUBLIC
    siligen_shared_kernel
)

target_compile_features(siligen_process_core PUBLIC cxx_std_17)
