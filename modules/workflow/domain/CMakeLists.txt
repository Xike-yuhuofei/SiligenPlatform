cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_process_planning_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../process-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/wd_ppc"
    )
endif()

# Bridge-only: workflow/domain 当前仍为迁移期桥接层。
# 允许保留的 sibling source 聚合仅限既有 owner 模块，禁止继续向外扩张。
set(SILIGEN_WORKFLOW_DOMAIN_BRIDGE_ONLY_ALLOWED_MODULES
    process-path
    motion-planning
    dispense-packaging
    coordinate-alignment
)

set(SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}/domain")
    message(FATAL_ERROR "workflow canonical domain include root missing: ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}/domain")
endif()
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

if(NOT TARGET siligen_coordinate_alignment_domain_machine)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../coordinate-alignment/domain/machine"
        "${CMAKE_BINARY_DIR}/modules/coordinate-alignment/domain/machine"
    )
endif()
if(NOT TARGET siligen_coordinate_alignment_domain_machine)
    message(FATAL_ERROR "workflow/domain machine bridge requires canonical target siligen_coordinate_alignment_domain_machine")
endif()

if(NOT TARGET domain_machine)
    add_library(domain_machine INTERFACE)
endif()
target_link_libraries(domain_machine INTERFACE
    siligen_coordinate_alignment_domain_machine
)
target_compile_features(domain_machine INTERFACE cxx_std_17)

if(NOT TARGET siligen_process_core)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/process-core"
        "${CMAKE_BINARY_DIR}/modules/workflow/domain/process-core"
    )
endif()

if(NOT TARGET siligen_motion_core)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/motion-core"
        "${CMAKE_BINARY_DIR}/modules/workflow/domain/motion-core"
    )
endif()

if(NOT TARGET siligen_process_path_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../process-path/contracts"
        "${CMAKE_BINARY_DIR}/_p/wd_pp_contracts"
    )
endif()

if(NOT TARGET siligen_motion)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../motion-planning/domain/motion"
        "${CMAKE_BINARY_DIR}/modules/motion-planning/domain/motion"
    )
endif()

if(NOT TARGET siligen_dispense_packaging_domain_dispensing)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../dispense-packaging/domain/dispensing"
        "${CMAKE_BINARY_DIR}/modules/dispense-packaging/domain/dispensing"
    )
endif()

if(NOT TARGET siligen_domain_services)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/domain"
        "${CMAKE_BINARY_DIR}/modules/workflow/domain/bridge-domain"
    )
endif()

if(NOT TARGET siligen_domain)
    add_library(siligen_domain INTERFACE)
endif()
target_include_directories(siligen_domain INTERFACE
    ${SILIGEN_WORKSPACE_PUBLIC_INCLUDE_DIRS}
)
target_link_libraries(siligen_domain INTERFACE
    siligen_workspace_boost_headers
    siligen_motion
    siligen_triggering
    siligen_configuration
    siligen_domain_services
    siligen_process_core
    siligen_motion_core
    siligen_process_path_contracts_public
    domain_recipes
    domain_machine
    siligen_dispense_packaging_domain_dispensing
    siligen_process_planning_contracts_public
)
target_compile_features(siligen_domain INTERFACE cxx_std_17)
