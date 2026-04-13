cmake_minimum_required(VERSION 3.20)

set(SILIGEN_WORKFLOW_ADAPTERS_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(required_dir
    IN ITEMS
        "${SILIGEN_WORKFLOW_ADAPTERS_ROOT_DIR}/persistence"
        "${SILIGEN_WORKFLOW_ADAPTERS_ROOT_DIR}/messaging"
        "${SILIGEN_WORKFLOW_ADAPTERS_ROOT_DIR}/projection_store"
)
    if(NOT EXISTS "${required_dir}")
        message(FATAL_ERROR "workflow canonical adapters slice missing: ${required_dir}")
    endif()
endforeach()
set(WORKFLOW_ADAPTERS_FORBIDDEN_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/infrastructure/adapters/redundancy"
    "${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/adapters/redundancy"
)
foreach(forbidden_dir IN LISTS WORKFLOW_ADAPTERS_FORBIDDEN_DIRS)
    if(EXISTS "${forbidden_dir}")
        message(FATAL_ERROR "workflow adapters legacy redundancy residue must stay deleted: ${forbidden_dir}")
    endif()
endforeach()

if(NOT TARGET siligen_workflow_adapters_headers)
    add_library(siligen_workflow_adapters_headers INTERFACE)
endif()
target_compile_features(siligen_workflow_adapters_headers INTERFACE cxx_std_17)
