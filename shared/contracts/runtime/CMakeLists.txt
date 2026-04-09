cmake_minimum_required(VERSION 3.20)

set(SILIGEN_RUNTIME_CONTRACTS_SHARED_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_RUNTIME_CONTRACTS_SHARED_INCLUDE_DIR}/runtime/contracts/system/IEventPublisherPort.h")
    message(FATAL_ERROR
        "shared/contracts/runtime canonical root requires runtime/contracts/system/IEventPublisherPort.h"
    )
endif()

if(NOT TARGET siligen_shared_runtime_contracts)
    add_library(siligen_shared_runtime_contracts INTERFACE)
endif()
target_include_directories(siligen_shared_runtime_contracts INTERFACE
    ${SILIGEN_RUNTIME_CONTRACTS_SHARED_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_shared_runtime_contracts INTERFACE
    siligen_types
)
target_compile_features(siligen_shared_runtime_contracts INTERFACE cxx_std_17)
set_target_properties(siligen_shared_runtime_contracts PROPERTIES
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "shared/contracts/runtime"
)
