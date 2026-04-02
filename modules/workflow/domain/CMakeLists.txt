cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_process_planning_legacy_configuration_bridge_public)
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
)

set(SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}/domain")
    message(FATAL_ERROR "workflow canonical domain include root missing: ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}/domain")
endif()
if(NOT DEFINED SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR)
    set(SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/../../motion-planning"
    )
endif()
if(NOT EXISTS "${SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR}/domain/motion")
    message(FATAL_ERROR "motion-planning canonical public include root missing: ${SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR}/domain/motion")
endif()

if(NOT TARGET siligen_workflow_domain_headers)
    add_library(siligen_workflow_domain_headers INTERFACE)
endif()
target_include_directories(siligen_workflow_domain_headers INTERFACE
    ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/process-core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/motion-core/include
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_compile_features(siligen_workflow_domain_headers INTERFACE cxx_std_17)

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

if(NOT TARGET siligen_process_path_domain_trajectory)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../process-path/domain/trajectory"
        "${CMAKE_BINARY_DIR}/modules/process-path/domain/trajectory"
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
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)
target_link_libraries(siligen_domain INTERFACE
    siligen_process_runtime_core_boost_headers
    siligen_motion
    siligen_triggering
    siligen_configuration
    siligen_domain_services
    siligen_process_core
    siligen_motion_core
    siligen_process_path_domain_trajectory
    domain_recipes
    domain_machine
    siligen_dispense_packaging_domain_dispensing
    siligen_process_planning_legacy_configuration_bridge_public
)
target_compile_features(siligen_domain INTERFACE cxx_std_17)
