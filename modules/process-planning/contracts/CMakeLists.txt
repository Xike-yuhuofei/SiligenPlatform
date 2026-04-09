cmake_minimum_required(VERSION 3.20)

set(SILIGEN_PROCESS_PLANNING_CONTRACTS_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(NOT EXISTS "${SILIGEN_PROCESS_PLANNING_CONTRACTS_PUBLIC_INCLUDE_DIR}/process_planning/contracts")
    message(FATAL_ERROR
        "process-planning canonical contracts include root missing: "
        "${SILIGEN_PROCESS_PLANNING_CONTRACTS_PUBLIC_INCLUDE_DIR}/process_planning/contracts"
    )
endif()

if(NOT TARGET siligen_process_planning_contracts_public)
    add_library(siligen_process_planning_contracts_public INTERFACE)
endif()
target_include_directories(siligen_process_planning_contracts_public INTERFACE
    ${SILIGEN_PROCESS_PLANNING_CONTRACTS_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
if(TARGET siligen_application_contracts)
    target_link_libraries(siligen_process_planning_contracts_public INTERFACE
        siligen_application_contracts
    )
endif()
target_compile_features(siligen_process_planning_contracts_public INTERFACE cxx_std_17)
