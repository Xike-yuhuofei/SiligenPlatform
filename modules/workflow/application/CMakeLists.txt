cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_process_planning_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../process-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/wa_ppc"
    )
endif()
if(NOT TARGET siligen_process_path_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../process-path/contracts"
        "${CMAKE_BINARY_DIR}/_p/wa_ppath"
    )
endif()
if(NOT TARGET siligen_motion_planning_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../motion-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/wa_mpc"
    )
endif()
if(NOT TARGET siligen_trace_diagnostics_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../trace-diagnostics/contracts"
        "${CMAKE_BINARY_DIR}/_p/wa_tdc"
    )
endif()
if(NOT TARGET siligen_motion_planning_application_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../motion-planning/application"
        "${CMAKE_BINARY_DIR}/_p/wa_mpa"
    )
endif()
if(NOT TARGET siligen_dispense_packaging_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../dispense-packaging/contracts"
        "${CMAKE_BINARY_DIR}/_p/wa_dpc"
    )
endif()
if(NOT TARGET siligen_dispense_packaging_domain_dispensing)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../dispense-packaging/domain/dispensing"
        "${CMAKE_BINARY_DIR}/_p/wa_dpd"
    )
endif()
if(NOT TARGET siligen_dispense_packaging_application_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../dispense-packaging/application"
        "${CMAKE_BINARY_DIR}/_p/wa_dpa"
    )
endif()
if(NOT TARGET siligen_job_ingest_contracts)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../job-ingest/contracts"
        "${CMAKE_BINARY_DIR}/_p/wa_jic"
    )
endif()
if(NOT TARGET siligen_runtime_execution_runtime_contracts)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime-execution/contracts/runtime"
        "${CMAKE_BINARY_DIR}/_p/wa_rec"
    )
endif()
if(NOT TARGET siligen_runtime_execution_application_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime-execution/application"
        "${CMAKE_BINARY_DIR}/_p/wa_rea"
    )
endif()
if(NOT TARGET siligen_device_contracts)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../shared/contracts/device"
        "${CMAKE_BINARY_DIR}/_p/wa_sdc"
    )
endif()
if(NOT TARGET siligen_shared_runtime_contracts)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../shared/contracts/runtime"
        "${CMAKE_BINARY_DIR}/_p/wa_src"
    )
endif()

# Bridge-only: workflow/application 当前仍为迁移期桥接层。
# 允许保留的跨模块 include/target 仅限既有 owner 模块，禁止继续新增 sibling bridge。
set(SILIGEN_WORKFLOW_APPLICATION_BRIDGE_ONLY_ALLOWED_MODULES
    process-path
    motion-planning
    dispense-packaging
    job-ingest
    dxf-geometry
    runtime-execution
)

set(SILIGEN_WORKFLOW_APPLICATION_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_WORKFLOW_APPLICATION_PUBLIC_INCLUDE_DIR}/application")
    message(FATAL_ERROR "workflow canonical application include root missing: ${SILIGEN_WORKFLOW_APPLICATION_PUBLIC_INCLUDE_DIR}/application")
endif()

if(NOT TARGET siligen_workflow_application_headers)
    add_library(siligen_workflow_application_headers INTERFACE)
endif()
target_include_directories(siligen_workflow_application_headers INTERFACE
    ${SILIGEN_WORKFLOW_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_compile_features(siligen_workflow_application_headers INTERFACE cxx_std_17)

set(SILIGEN_APPLICATION_PUBLIC_INCLUDE_DIRS
    ${SILIGEN_WORKSPACE_PUBLIC_INCLUDE_DIRS}
)
set(SILIGEN_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS
    ${SILIGEN_WORKSPACE_IMPLEMENTATION_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../process-path/application/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../motion-planning/application/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dispense-packaging/application/include
)

add_library(siligen_application_dispensing STATIC
    ports/dispensing/PlanningPortAdapters.cpp
    planning-trigger/PlanningUseCase.cpp
    phase-control/DispensingWorkflowUseCase.cpp
)
target_include_directories(siligen_application_dispensing PUBLIC
    ${SILIGEN_APPLICATION_PUBLIC_INCLUDE_DIRS}
)
target_include_directories(siligen_application_dispensing PRIVATE
    ${SILIGEN_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_application_dispensing PUBLIC
    siligen_workflow_application_headers
    siligen_workflow_contracts_public
    siligen_workflow_domain_public
    siligen_types
    siligen_utils
    siligen_geometry
    siligen_dispense_packaging_domain_dispensing
)
if(TARGET siligen_process_path_contracts_public)
    target_link_libraries(siligen_application_dispensing PUBLIC
        siligen_process_path_contracts_public
    )
endif()
if(TARGET siligen_motion_planning_contracts_public)
    target_link_libraries(siligen_application_dispensing PUBLIC
        siligen_motion_planning_contracts_public
    )
endif()
if(TARGET siligen_device_contracts)
    target_link_libraries(siligen_application_dispensing PUBLIC
        siligen_device_contracts
    )
endif()
if(TARGET siligen_shared_runtime_contracts)
    target_link_libraries(siligen_application_dispensing PUBLIC
        siligen_shared_runtime_contracts
    )
endif()
if(TARGET siligen_runtime_execution_runtime_contracts)
    target_link_libraries(siligen_application_dispensing PUBLIC
        siligen_runtime_execution_runtime_contracts
    )
endif()
if(TARGET siligen_process_path_application_public)
    target_link_libraries(siligen_application_dispensing PRIVATE
        siligen_process_path_application_public
    )
endif()
if(TARGET siligen_motion_planning_application_public)
    target_link_libraries(siligen_application_dispensing PRIVATE
        siligen_motion_planning_application_public
    )
endif()
if(TARGET siligen_dispense_packaging_application_public)
    target_link_libraries(siligen_application_dispensing PRIVATE
        siligen_dispense_packaging_application_public
    )
endif()
if(TARGET siligen_dispense_packaging_contracts_public)
    target_link_libraries(siligen_application_dispensing PUBLIC
        siligen_dispense_packaging_contracts_public
    )
endif()
if(TARGET siligen_job_ingest_contracts)
    target_link_libraries(siligen_application_dispensing PUBLIC
        siligen_job_ingest_contracts
    )
endif()
if(TARGET siligen_dxf_geometry_application_public)
    target_link_libraries(siligen_application_dispensing PRIVATE
        siligen_dxf_geometry_application_public
    )
endif()
if(TARGET siligen_runtime_execution_application_public)
    # runtime planning export wiring is implementation-only and must not leak
    # back out through the workflow public header bundle.
    target_link_libraries(siligen_application_dispensing PRIVATE
        siligen_runtime_execution_application_public
    )
endif()
target_compile_features(siligen_application_dispensing PUBLIC cxx_std_17)

add_library(siligen_application_redundancy STATIC
    recovery-control/CandidateScoringService.cpp
    recovery-control/RedundancyGovernanceUseCases.cpp
)
target_include_directories(siligen_application_redundancy PUBLIC
    ${SILIGEN_APPLICATION_PUBLIC_INCLUDE_DIRS}
)
target_include_directories(siligen_application_redundancy PRIVATE
    ${SILIGEN_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_application_redundancy PUBLIC
    siligen_workflow_application_headers
    siligen_types
    siligen_utils
    siligen_workflow_domain_headers
)
target_compile_features(siligen_application_redundancy PUBLIC cxx_std_17)

# CGAL (填充算法)
target_compile_definitions(siligen_application_dispensing PRIVATE
    SILIGEN_ENABLE_CGAL=0
    SILIGEN_TEST_HOOKS=1
)

# 为应用层库添加预编译头
if(SILIGEN_USE_PCH)
    message(STATUS
        "[Application Layer] workflow/application 全部目标跳过预编译头，"
        "以规避 Windows/MSBuild 下的 PCH 文件锁竞争"
    )
    message(STATUS "[Application Layer] 预编译头已对 workflow/application 全部禁用")
endif()

message(STATUS "[Application Layer] 应用层配置完成")
