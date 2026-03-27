cmake_minimum_required(VERSION 3.20)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(shared-contracts-entry LANGUAGES CXX)
endif()

get_filename_component(SILIGEN_SHARED_CONTRACTS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(SILIGEN_SHARED_CONTRACTS_WORKSPACE_ROOT_CANDIDATE "${SILIGEN_SHARED_CONTRACTS_SOURCE_DIR}/../.." ABSOLUTE)

if(NOT DEFINED SILIGEN_WORKSPACE_ROOT OR NOT EXISTS "${SILIGEN_WORKSPACE_ROOT}/cmake/LoadWorkspaceLayout.cmake")
    set(SILIGEN_LAYOUT_FILE "${SILIGEN_SHARED_CONTRACTS_WORKSPACE_ROOT_CANDIDATE}/cmake/workspace-layout.env")
    include("${SILIGEN_SHARED_CONTRACTS_WORKSPACE_ROOT_CANDIDATE}/cmake/LoadWorkspaceLayout.cmake")
    siligen_load_workspace_layout(
        "${SILIGEN_SHARED_CONTRACTS_WORKSPACE_ROOT_CANDIDATE}"
        "${SILIGEN_LAYOUT_FILE}"
    )
    set(SILIGEN_WORKSPACE_ROOT "${SILIGEN_SHARED_CONTRACTS_WORKSPACE_ROOT_CANDIDATE}")
endif()

if(NOT "${SILIGEN_WORKSPACE_ROOT}" STREQUAL "${SILIGEN_SHARED_CONTRACTS_WORKSPACE_ROOT_CANDIDATE}")
    message(FATAL_ERROR
        "shared/contracts root mismatch: "
        "resolved='${SILIGEN_WORKSPACE_ROOT}', candidate='${SILIGEN_SHARED_CONTRACTS_WORKSPACE_ROOT_CANDIDATE}'"
    )
endif()

if(NOT TARGET siligen_application_contracts)
    add_library(siligen_application_contracts INTERFACE)
    target_compile_features(siligen_application_contracts INTERFACE cxx_std_17)
endif()
set_target_properties(siligen_application_contracts PROPERTIES
    SILIGEN_MIGRATION_SOURCE_ROOT "legacy-archive/application-contracts"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "shared/contracts/application"
)

if(NOT TARGET siligen_engineering_contracts)
    add_library(siligen_engineering_contracts INTERFACE)
    target_compile_features(siligen_engineering_contracts INTERFACE cxx_std_17)
endif()
set_target_properties(siligen_engineering_contracts PROPERTIES
    SILIGEN_MIGRATION_SOURCE_ROOT "legacy-archive/engineering-contracts"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "shared/contracts/engineering"
)

set(SILIGEN_DEVICE_CONTRACTS_CANONICAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/device")
if(NOT EXISTS "${SILIGEN_DEVICE_CONTRACTS_CANONICAL_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "未找到 canonical shared/contracts/device source: "
        "${SILIGEN_DEVICE_CONTRACTS_CANONICAL_DIR}/CMakeLists.txt"
    )
endif()

if(NOT TARGET siligen_device_contracts)
    add_subdirectory(
        "${SILIGEN_DEVICE_CONTRACTS_CANONICAL_DIR}"
        "${CMAKE_BINARY_DIR}/shared/contracts/device"
    )
endif()

if(NOT TARGET siligen_device_contracts)
    message(FATAL_ERROR "shared/contracts bridge 未注册 target: siligen_device_contracts")
endif()

set_target_properties(siligen_device_contracts PROPERTIES
    SILIGEN_MIGRATION_SOURCE_ROOT "legacy-archive/device-contracts"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "shared/contracts/device"
)

if(NOT TARGET siligen_shared_contracts)
    add_library(siligen_shared_contracts INTERFACE)
endif()

target_link_libraries(siligen_shared_contracts INTERFACE
    siligen_application_contracts
    siligen_engineering_contracts
    siligen_device_contracts
)
target_compile_features(siligen_shared_contracts INTERFACE cxx_std_17)
