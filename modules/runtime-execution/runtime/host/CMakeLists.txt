cmake_minimum_required(VERSION 3.20)

set(RUNTIME_HOST_DEVICE_ADAPTERS_CANONICAL_DIR "${SILIGEN_WORKSPACE_ROOT}/modules/runtime-execution/adapters/device")
set(RUNTIME_HOST_DEVICE_CONTRACTS_CANONICAL_DIR "${SILIGEN_WORKSPACE_ROOT}/shared/contracts/device")
set(RUNTIME_HOST_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(RUNTIME_HOST_TRACEABILITY_DIR "${SILIGEN_WORKSPACE_ROOT}/modules/trace-diagnostics")
set(RUNTIME_HOST_WORKFLOW_APPLICATION_INCLUDE_DIR
    "${SILIGEN_WORKSPACE_ROOT}/modules/workflow/application/include")
set(RUNTIME_HOST_DEVICE_ADAPTERS_INCLUDE_DIR "${RUNTIME_HOST_DEVICE_ADAPTERS_CANONICAL_DIR}/include")
set(RUNTIME_HOST_DEVICE_CONTRACTS_INCLUDE_DIR "${RUNTIME_HOST_DEVICE_CONTRACTS_CANONICAL_DIR}/include")

if(NOT EXISTS "${RUNTIME_HOST_SOURCE_ROOT}/runtime/diagnostics/DiagnosticsPortAdapter.cpp")
    message(FATAL_ERROR
        "runtime-execution canonical runtime host now requires runtime/host as the host-core source root."
    )
endif()

if(NOT TARGET siligen_device_contracts AND EXISTS "${RUNTIME_HOST_DEVICE_CONTRACTS_CANONICAL_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_DEVICE_CONTRACTS_CANONICAL_DIR}"
        "${CMAKE_BINARY_DIR}/_p/rh_sdc"
    )
endif()

if(NOT TARGET siligen_device_adapters AND EXISTS "${RUNTIME_HOST_DEVICE_ADAPTERS_CANONICAL_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_DEVICE_ADAPTERS_CANONICAL_DIR}"
        "${CMAKE_BINARY_DIR}/modules/runtime-execution/adapters/device"
    )
endif()

if(NOT TARGET siligen_trace_diagnostics_application_public AND EXISTS "${RUNTIME_HOST_TRACEABILITY_DIR}/application/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_TRACEABILITY_DIR}/application"
        "${CMAKE_BINARY_DIR}/_p/rh_tda"
    )
endif()

if(NOT TARGET siligen_runtime_host_boost_headers)
    add_library(siligen_runtime_host_boost_headers INTERFACE)
    target_include_directories(siligen_runtime_host_boost_headers INTERFACE
        ${SILIGEN_BOOST_INCLUDE_DIR}
    )
    target_compile_features(siligen_runtime_host_boost_headers INTERFACE cxx_std_17)
endif()

if(NOT TARGET siligen_event_adapters AND EXISTS "${RUNTIME_HOST_SOURCE_ROOT}/runtime/events/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_SOURCE_ROOT}/runtime/events"
        "${CMAKE_BINARY_DIR}/modules/runtime-execution/runtime/host/runtime-events"
    )
endif()

if(NOT TARGET siligen_task_scheduler AND EXISTS "${RUNTIME_HOST_SOURCE_ROOT}/runtime/scheduling/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_SOURCE_ROOT}/runtime/scheduling"
        "${CMAKE_BINARY_DIR}/modules/runtime-execution/runtime/host/runtime-scheduling"
    )
endif()

set(RUNTIME_HOST_PUBLIC_INCLUDE_DIRS
    ${RUNTIME_HOST_SOURCE_ROOT}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
    ${RUNTIME_HOST_WORKFLOW_APPLICATION_INCLUDE_DIR}
)
list(REMOVE_DUPLICATES RUNTIME_HOST_PUBLIC_INCLUDE_DIRS)

add_library(siligen_runtime_host STATIC
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/diagnostics/DiagnosticsPortAdapter.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/supervision/RuntimeSupervisionPortAdapter.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/system/LegacyMachineExecutionStateAdapter.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/motion/WorkflowMotionRuntimeServicesProvider.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/planning/PlanningArtifactExportPortAdapter.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/services/motion/HardLimitMonitorService.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/services/motion/SoftLimitMonitorService.cpp
)

set_target_properties(siligen_runtime_host PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

target_include_directories(siligen_runtime_host PUBLIC
    ${RUNTIME_HOST_PUBLIC_INCLUDE_DIRS}
)

target_link_libraries(siligen_runtime_host PUBLIC
    siligen_runtime_execution_application_public
    siligen_runtime_execution_runtime_contracts
    siligen_device_contracts
    siligen_domain
    siligen_shared
)

if(TARGET siligen_dispense_packaging_application_public)
    target_link_libraries(siligen_runtime_host PUBLIC
        siligen_dispense_packaging_application_public
    )
endif()

target_link_libraries(siligen_runtime_host PRIVATE
    siligen_types
    siligen_utils
)

set_target_properties(siligen_runtime_host PROPERTIES
    SILIGEN_MIGRATION_SOURCE_ROOT "legacy-archive/runtime-host"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/runtime-execution"
)
