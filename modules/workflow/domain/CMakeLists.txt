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

message(STATUS "[Workflow::Domain] bridge root exports headers only; deleted machine/runtime compat targets stay absent")
