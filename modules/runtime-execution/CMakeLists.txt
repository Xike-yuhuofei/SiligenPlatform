cmake_minimum_required(VERSION 3.20)

set(SILIGEN_RUNTIME_EXECUTION_DEVICE_CONTRACTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/contracts/device")
set(SILIGEN_RUNTIME_EXECUTION_DEVICE_ADAPTERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/adapters/device")
set(SILIGEN_RUNTIME_EXECUTION_RUNTIME_HOST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/runtime/host")

if(NOT TARGET siligen_device_contracts AND EXISTS "${SILIGEN_RUNTIME_EXECUTION_DEVICE_CONTRACTS_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${SILIGEN_RUNTIME_EXECUTION_DEVICE_CONTRACTS_DIR}"
        "${CMAKE_BINARY_DIR}/modules/runtime-execution/contracts/device"
    )
endif()

if(NOT TARGET siligen_device_adapters AND EXISTS "${SILIGEN_RUNTIME_EXECUTION_DEVICE_ADAPTERS_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${SILIGEN_RUNTIME_EXECUTION_DEVICE_ADAPTERS_DIR}"
        "${CMAKE_BINARY_DIR}/modules/runtime-execution/adapters/device"
    )
endif()

if(NOT TARGET siligen_runtime_host AND EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_HOST_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_HOST_DIR}"
        "${CMAKE_BINARY_DIR}/modules/runtime-execution/runtime/host"
    )
endif()

if(NOT TARGET siligen_module_runtime_execution)
    add_library(siligen_module_runtime_execution INTERFACE)
endif()
target_link_libraries(siligen_module_runtime_execution INTERFACE siligen_runtime_host)
target_compile_features(siligen_module_runtime_execution INTERFACE cxx_std_17)

set_target_properties(siligen_module_runtime_execution PROPERTIES
    SILIGEN_MODULE_ID "M9"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/runtime-execution"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/runtime-execution/domain;modules/runtime-execution/services;modules/runtime-execution/application;modules/runtime-execution/adapters;modules/runtime-execution/runtime;modules/runtime-execution/tests;modules/runtime-execution/examples"
)
