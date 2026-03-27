cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_process_planning_domain_configuration)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../domain/configuration"
        "${CMAKE_CURRENT_BINARY_DIR}/odc"
    )
endif()

set(SILIGEN_PROCESS_PLANNING_CONTRACTS_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SILIGEN_PROCESS_PLANNING_LEGACY_BRIDGE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/legacy-bridge/include")

if(NOT EXISTS "${SILIGEN_PROCESS_PLANNING_CONTRACTS_PUBLIC_INCLUDE_DIR}/process_planning/contracts")
    message(FATAL_ERROR
        "process-planning canonical contracts include root missing: "
        "${SILIGEN_PROCESS_PLANNING_CONTRACTS_PUBLIC_INCLUDE_DIR}/process_planning/contracts"
    )
endif()

if(NOT EXISTS "${SILIGEN_PROCESS_PLANNING_LEGACY_BRIDGE_INCLUDE_DIR}/domain/configuration/ports/IConfigurationPort.h")
    message(FATAL_ERROR
        "process-planning canonical legacy bridge include root missing: "
        "${SILIGEN_PROCESS_PLANNING_LEGACY_BRIDGE_INCLUDE_DIR}/domain/configuration/ports/IConfigurationPort.h"
    )
endif()

if(NOT TARGET siligen_process_planning_contracts_public)
    add_library(siligen_process_planning_contracts_public INTERFACE)
endif()
target_include_directories(siligen_process_planning_contracts_public INTERFACE
    ${SILIGEN_PROCESS_PLANNING_CONTRACTS_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_PROCESS_PLANNING_LEGACY_BRIDGE_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_process_planning_contracts_public INTERFACE
    siligen_process_planning_domain_configuration
)
if(TARGET siligen_application_contracts)
    target_link_libraries(siligen_process_planning_contracts_public INTERFACE
        siligen_application_contracts
    )
endif()
target_compile_features(siligen_process_planning_contracts_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_process_planning_legacy_configuration_bridge_public)
    add_library(siligen_process_planning_legacy_configuration_bridge_public INTERFACE)
endif()
target_include_directories(siligen_process_planning_legacy_configuration_bridge_public INTERFACE
    ${SILIGEN_PROCESS_PLANNING_LEGACY_BRIDGE_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_process_planning_legacy_configuration_bridge_public INTERFACE
    siligen_process_planning_contracts_public
    siligen_process_planning_domain_configuration
)
target_compile_features(siligen_process_planning_legacy_configuration_bridge_public INTERFACE cxx_std_17)
