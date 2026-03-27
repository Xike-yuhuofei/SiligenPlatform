cmake_minimum_required(VERSION 3.20)

set(SILIGEN_SHARED_DEVICE_CONTRACTS_CANONICAL_DIR "${SILIGEN_WORKSPACE_ROOT}/shared/contracts/device")
if(NOT EXISTS "${SILIGEN_SHARED_DEVICE_CONTRACTS_CANONICAL_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "shared/contracts/device canonical target missing: "
        "${SILIGEN_SHARED_DEVICE_CONTRACTS_CANONICAL_DIR}/CMakeLists.txt"
    )
endif()

if(NOT TARGET siligen_device_contracts)
    add_subdirectory(
        "${SILIGEN_SHARED_DEVICE_CONTRACTS_CANONICAL_DIR}"
        "${CMAKE_BINARY_DIR}/shared/contracts/device"
    )
endif()

if(NOT TARGET siligen_device_contracts)
    message(FATAL_ERROR
        "shared/contracts/device did not register siligen_device_contracts"
    )
endif()

if(NOT TARGET siligen_runtime_execution_device_contracts)
    add_library(siligen_runtime_execution_device_contracts INTERFACE)
endif()
target_link_libraries(siligen_runtime_execution_device_contracts INTERFACE
    siligen_device_contracts
)
target_compile_features(siligen_runtime_execution_device_contracts INTERFACE cxx_std_17)
set_target_properties(siligen_runtime_execution_device_contracts PROPERTIES
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/runtime-execution"
)
