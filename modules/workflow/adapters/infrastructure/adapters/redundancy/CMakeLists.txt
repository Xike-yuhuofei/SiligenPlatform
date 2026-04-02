add_library(siligen_redundancy_storage_adapter STATIC
    JsonRedundancyRepositoryAdapter.cpp
)

target_include_directories(siligen_redundancy_storage_adapter PUBLIC
    ${WORKFLOW_MODULE_PUBLIC_INCLUDE_DIRS}
)
target_include_directories(siligen_redundancy_storage_adapter PRIVATE
    ${WORKFLOW_MODULE_IMPLEMENTATION_INCLUDE_DIRS}
)

target_link_libraries(siligen_redundancy_storage_adapter PUBLIC
    siligen_shared_json_headers
    siligen_types
    siligen_utils
)

target_compile_features(siligen_redundancy_storage_adapter PUBLIC cxx_std_17)
