cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_runtime_execution_runtime_contracts)
    add_library(siligen_runtime_execution_runtime_contracts INTERFACE)
endif()
if(NOT TARGET siligen_shared_runtime_contracts)
    add_subdirectory(
        "${SILIGEN_WORKSPACE_ROOT}/shared/contracts/runtime"
        "${CMAKE_BINARY_DIR}/_p/rerc_src"
    )
endif()
if(NOT TARGET siligen_process_path_contracts_public)
    add_subdirectory(
        "${SILIGEN_WORKSPACE_ROOT}/modules/process-path/contracts"
        "${CMAKE_BINARY_DIR}/_p/rerc_ppath"
    )
endif()
if(NOT TARGET siligen_motion_planning_contracts_public)
    add_subdirectory(
        "${SILIGEN_WORKSPACE_ROOT}/modules/motion-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/rerc_mpc"
    )
endif()

set(SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SILIGEN_DISPENSE_PACKAGING_CONTRACTS_INCLUDE_DIR
    "${SILIGEN_WORKSPACE_ROOT}/modules/dispense-packaging/contracts/include"
)
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}/runtime_execution/contracts/system/IRuntimeSupervisionPort.h")
    message(FATAL_ERROR
        "runtime-execution canonical runtime contracts require runtime_execution/contracts/system/IRuntimeSupervisionPort.h"
    )
endif()
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}/runtime_execution/contracts/motion/IMotionRuntimePort.h")
    message(FATAL_ERROR
        "runtime-execution canonical runtime contracts require runtime_execution/contracts/motion/IMotionRuntimePort.h"
    )
endif()
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}/runtime_execution/contracts/motion/IMotionConnectionPort.h")
    message(FATAL_ERROR
        "runtime-execution canonical runtime contracts require runtime_execution/contracts/motion/IMotionConnectionPort.h"
    )
endif()
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}/runtime_execution/contracts/motion/IAxisControlPort.h")
    message(FATAL_ERROR
        "runtime-execution canonical runtime contracts require runtime_execution/contracts/motion/IAxisControlPort.h"
    )
endif()
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}/runtime_execution/contracts/motion/IJogControlPort.h")
    message(FATAL_ERROR
        "runtime-execution canonical runtime contracts require runtime_execution/contracts/motion/IJogControlPort.h"
    )
endif()
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}/runtime_execution/contracts/motion/IIOControlPort.h")
    message(FATAL_ERROR
        "runtime-execution canonical runtime contracts require runtime_execution/contracts/motion/IIOControlPort.h"
    )
endif()
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}/runtime_execution/contracts/motion/IInterpolationPort.h")
    message(FATAL_ERROR
        "runtime-execution canonical runtime contracts require runtime_execution/contracts/motion/IInterpolationPort.h"
    )
endif()
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}/runtime_execution/contracts/dispensing/ITriggerControllerPort.h")
    message(FATAL_ERROR
        "runtime-execution canonical runtime contracts require runtime_execution/contracts/dispensing/ITriggerControllerPort.h"
    )
endif()
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}/runtime_execution/contracts/dispensing/DispenseCompensationProfile.h")
    message(FATAL_ERROR
        "runtime-execution canonical runtime contracts require runtime_execution/contracts/dispensing/DispenseCompensationProfile.h"
    )
endif()
if(NOT EXISTS "${SILIGEN_DISPENSE_PACKAGING_CONTRACTS_INCLUDE_DIR}/dispense_packaging/contracts/IValvePort.h")
    message(FATAL_ERROR
        "runtime-execution canonical runtime contracts require dispense-packaging contracts include root: "
        "${SILIGEN_DISPENSE_PACKAGING_CONTRACTS_INCLUDE_DIR}"
    )
endif()

target_include_directories(siligen_runtime_execution_runtime_contracts INTERFACE
    ${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}
    ${SILIGEN_DISPENSE_PACKAGING_CONTRACTS_INCLUDE_DIR}
)
target_link_libraries(siligen_runtime_execution_runtime_contracts INTERFACE
    siligen_shared_runtime_contracts
    siligen_motion_planning_contracts_public
    siligen_process_path_contracts_public
)
target_compile_features(siligen_runtime_execution_runtime_contracts INTERFACE cxx_std_17)
set_target_properties(siligen_runtime_execution_runtime_contracts PROPERTIES
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/runtime-execution"
)
