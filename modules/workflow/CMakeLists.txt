cmake_minimum_required(VERSION 3.20)

set(SILIGEN_WORKFLOW_APPLICATION_PUBLIC_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/application/include"
)
set(SILIGEN_WORKFLOW_CONTRACTS_PUBLIC_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/contracts/include"
)
set(SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/domain/include"
)
set(SILIGEN_WORKFLOW_ADAPTERS_PUBLIC_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/adapters/include"
)
if(NOT DEFINED SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR)
    set(SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/../motion-planning"
    )
endif()
foreach(required_dir
    IN ITEMS
        "${SILIGEN_WORKFLOW_APPLICATION_PUBLIC_INCLUDE_DIR}/workflow/application"
        "${SILIGEN_WORKFLOW_CONTRACTS_PUBLIC_INCLUDE_DIR}/workflow/contracts"
        "${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}/domain"
        "${SILIGEN_WORKFLOW_ADAPTERS_PUBLIC_INCLUDE_DIR}/infrastructure"
        "${SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR}/domain/motion"
)
    if(NOT EXISTS "${required_dir}")
        message(FATAL_ERROR "workflow canonical public include root missing: ${required_dir}")
    endif()
endforeach()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/domain" "${CMAKE_BINARY_DIR}/modules/workflow/domain")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/adapters" "${CMAKE_BINARY_DIR}/modules/workflow/adapters")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/application" "${CMAKE_BINARY_DIR}/modules/workflow/application")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/contracts" "${CMAKE_BINARY_DIR}/modules/workflow/contracts")

if(NOT TARGET siligen_workflow_domain_public)
    add_library(siligen_workflow_domain_public INTERFACE)
endif()
target_link_libraries(siligen_workflow_domain_public INTERFACE
    siligen_workflow_domain_headers
)
target_compile_features(siligen_workflow_domain_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_workflow_contracts_public)
    add_library(siligen_workflow_contracts_public INTERFACE)
endif()
target_link_libraries(siligen_workflow_contracts_public INTERFACE
    siligen_workflow_contracts_headers
)
target_compile_features(siligen_workflow_contracts_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_workflow_adapters_public)
    add_library(siligen_workflow_adapters_public INTERFACE)
endif()
target_link_libraries(siligen_workflow_adapters_public INTERFACE
    siligen_workflow_domain_public
    siligen_workflow_adapters_headers
)
foreach(adapter_target
    IN ITEMS
        siligen_redundancy_storage_adapter
)
    if(TARGET ${adapter_target})
        target_link_libraries(siligen_workflow_adapters_public INTERFACE
            $<LINK_ONLY:${adapter_target}>
        )
    endif()
endforeach()
target_compile_features(siligen_workflow_adapters_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_module_workflow)
    add_library(siligen_module_workflow INTERFACE)
endif()
target_link_libraries(siligen_module_workflow INTERFACE
    siligen_workflow_contracts_public
    siligen_workflow_domain_public
    siligen_workflow_application_headers
    siligen_workflow_adapters_public
)
foreach(workflow_application_target
    IN ITEMS
        siligen_application_dispensing
        siligen_application_redundancy
)
    if(TARGET ${workflow_application_target})
        target_link_libraries(siligen_module_workflow INTERFACE
            $<LINK_ONLY:${workflow_application_target}>
        )
    endif()
endforeach()
target_compile_features(siligen_module_workflow INTERFACE cxx_std_17)

set_target_properties(siligen_module_workflow PROPERTIES
    SILIGEN_MODULE_ID "M0"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/workflow"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/workflow/domain;modules/workflow/services;modules/workflow/application;modules/workflow/adapters;modules/workflow/tests;modules/workflow/examples"
)
