cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_shared_kernel AND EXISTS "${SILIGEN_WORKSPACE_ROOT}/shared/kernel/CMakeLists.txt")
    add_subdirectory(
        "${SILIGEN_WORKSPACE_ROOT}/shared/kernel"
        "${CMAKE_BINARY_DIR}/shared/kernel"
    )
endif()

set(SILIGEN_DEVICE_CONTRACTS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_DEVICE_CONTRACTS_INCLUDE_DIR}/siligen/device/contracts/device_contracts.h")
    message(FATAL_ERROR
        "shared/contracts/device canonical root requires include/siligen/device/contracts/device_contracts.h"
    )
endif()

if(NOT TARGET siligen_device_contracts)
    add_library(siligen_device_contracts INTERFACE)
endif()

target_include_directories(siligen_device_contracts INTERFACE
    ${SILIGEN_DEVICE_CONTRACTS_INCLUDE_DIR}
)
target_link_libraries(siligen_device_contracts INTERFACE
    siligen_shared_kernel
)
target_compile_features(siligen_device_contracts INTERFACE cxx_std_17)
set_target_properties(siligen_device_contracts PROPERTIES
    SILIGEN_MIGRATION_SOURCE_ROOT "legacy-archive/device-contracts"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "shared/contracts/device"
)
