cmake_minimum_required(VERSION 3.20)

set(SILIGEN_WORKFLOW_ADAPTERS_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
foreach(required_dir
    IN ITEMS
        "${SILIGEN_WORKFLOW_ADAPTERS_PUBLIC_INCLUDE_DIR}/infrastructure"
)
    if(NOT EXISTS "${required_dir}")
        message(FATAL_ERROR "workflow canonical adapters include root missing: ${required_dir}")
    endif()
endforeach()

if(NOT TARGET siligen_workflow_adapters_headers)
    add_library(siligen_workflow_adapters_headers INTERFACE)
endif()
target_include_directories(siligen_workflow_adapters_headers INTERFACE
    ${SILIGEN_WORKFLOW_ADAPTERS_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_compile_features(siligen_workflow_adapters_headers INTERFACE cxx_std_17)

if(NOT TARGET siligen_redundancy_storage_adapter)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/adapters/redundancy"
        "${CMAKE_BINARY_DIR}/modules/workflow/adapters/bridge-redundancy"
    )
endif()
