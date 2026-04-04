cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_runtime_execution_runtime_contracts)
    add_library(siligen_runtime_execution_runtime_contracts INTERFACE)
endif()

set(SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}/runtime_execution/contracts/system/IEventPublisherPort.h")
    message(FATAL_ERROR
        "runtime-execution canonical runtime contracts require runtime_execution/contracts/system/IEventPublisherPort.h"
    )
endif()
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}/runtime_execution/contracts/system/IRuntimeSupervisionPort.h")
    message(FATAL_ERROR
        "runtime-execution canonical runtime contracts require runtime_execution/contracts/system/IRuntimeSupervisionPort.h"
    )
endif()
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}/runtime_execution/contracts/dispensing/ITaskSchedulerPort.h")
    message(FATAL_ERROR
        "runtime-execution canonical runtime contracts require runtime_execution/contracts/dispensing/ITaskSchedulerPort.h"
    )
endif()
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}/runtime_execution/contracts/motion/IMotionRuntimePort.h")
    message(FATAL_ERROR
        "runtime-execution canonical runtime contracts require runtime_execution/contracts/motion/IMotionRuntimePort.h"
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

target_include_directories(siligen_runtime_execution_runtime_contracts INTERFACE
    ${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}
)
target_link_libraries(siligen_runtime_execution_runtime_contracts INTERFACE
    siligen_domain
)
target_compile_features(siligen_runtime_execution_runtime_contracts INTERFACE cxx_std_17)
set_target_properties(siligen_runtime_execution_runtime_contracts PROPERTIES
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/runtime-execution"
)
