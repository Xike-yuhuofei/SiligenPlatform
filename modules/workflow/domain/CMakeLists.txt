cmake_minimum_required(VERSION 3.20)

set(SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
foreach(required_dir
    IN ITEMS
        "${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}/workflow/domain/workflow_run"
        "${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}/workflow/domain/stage_transition"
        "${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}/workflow/domain/rollback"
        "${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}/workflow/domain/policies"
        "${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}/workflow/domain/ports"
)
    if(NOT EXISTS "${required_dir}")
        message(FATAL_ERROR "workflow canonical owner domain slice missing: ${required_dir}")
    endif()
endforeach()
set(WORKFLOW_DOMAIN_LEGACY_RESIDUE_ROOTS
    "${CMAKE_CURRENT_SOURCE_DIR}/domain"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/domain"
    "${CMAKE_CURRENT_SOURCE_DIR}/motion-core"
)
foreach(legacy_dir IN LISTS WORKFLOW_DOMAIN_LEGACY_RESIDUE_ROOTS)
    if(EXISTS "${legacy_dir}")
        message(FATAL_ERROR "workflow domain legacy residue root must stay deleted: ${legacy_dir}")
    endif()
endforeach()
if(NOT TARGET siligen_workflow_domain_headers)
    add_library(siligen_workflow_domain_headers INTERFACE)
endif()
target_include_directories(siligen_workflow_domain_headers INTERFACE
    ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}
)
target_link_libraries(siligen_workflow_domain_headers INTERFACE
    siligen_types
)
target_compile_features(siligen_workflow_domain_headers INTERFACE cxx_std_17)

message(STATUS "[Workflow::Domain] exports canonical M0 owner headers only")
