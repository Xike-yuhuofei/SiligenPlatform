cmake_minimum_required(VERSION 3.20)

set(SILIGEN_WORKFLOW_CONTRACTS_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_WORKFLOW_CONTRACTS_PUBLIC_INCLUDE_DIR}/workflow/contracts")
    message(FATAL_ERROR
        "workflow canonical contracts include root missing: "
        "${SILIGEN_WORKFLOW_CONTRACTS_PUBLIC_INCLUDE_DIR}/workflow/contracts"
    )
endif()

if(NOT TARGET siligen_workflow_contracts_headers)
    add_library(siligen_workflow_contracts_headers INTERFACE)
endif()
target_include_directories(siligen_workflow_contracts_headers INTERFACE
    ${SILIGEN_WORKFLOW_CONTRACTS_PUBLIC_INCLUDE_DIR}
)
target_compile_features(siligen_workflow_contracts_headers INTERFACE cxx_std_17)
