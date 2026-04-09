# Recipes subdomain CMakeLists.txt

add_library(domain_recipes INTERFACE)

target_include_directories(domain_recipes INTERFACE
    ${SILIGEN_WORKSPACE_PUBLIC_INCLUDE_DIRS}
)

target_link_libraries(domain_recipes INTERFACE
    siligen_types
)

target_compile_features(domain_recipes INTERFACE cxx_std_17)

message(STATUS "[Domain::Recipes] domain_recipes interface library configured")
