cmake_minimum_required(VERSION 3.20)

set(SILIGEN_DISPENSE_PACKAGING_CONTRACTS_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_DISPENSE_PACKAGING_CONTRACTS_PUBLIC_INCLUDE_DIR}/domain/dispensing/contracts/ExecutionPackage.h")
    message(FATAL_ERROR
        "dispense-packaging canonical contracts header missing: "
        "${SILIGEN_DISPENSE_PACKAGING_CONTRACTS_PUBLIC_INCLUDE_DIR}/domain/dispensing/contracts/ExecutionPackage.h"
    )
endif()
if(NOT EXISTS "${SILIGEN_DISPENSE_PACKAGING_CONTRACTS_PUBLIC_INCLUDE_DIR}/domain/dispensing/contracts/PlanningArtifactExportRequest.h")
    message(FATAL_ERROR
        "dispense-packaging canonical contracts header missing: "
        "${SILIGEN_DISPENSE_PACKAGING_CONTRACTS_PUBLIC_INCLUDE_DIR}/domain/dispensing/contracts/PlanningArtifactExportRequest.h"
    )
endif()

if(NOT TARGET siligen_dispense_packaging_contracts_public)
    add_library(siligen_dispense_packaging_contracts_public INTERFACE)
endif()

if(NOT TARGET siligen_process_path_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../process-path/contracts"
        "${CMAKE_BINARY_DIR}/_p/dpc_ppath"
    )
endif()

target_include_directories(siligen_dispense_packaging_contracts_public INTERFACE
    ${SILIGEN_DISPENSE_PACKAGING_CONTRACTS_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)

target_link_libraries(siligen_dispense_packaging_contracts_public INTERFACE
    siligen_process_path_contracts_public
)

if(TARGET siligen_application_contracts)
    target_link_libraries(siligen_dispense_packaging_contracts_public INTERFACE
        siligen_application_contracts
    )
endif()

target_compile_features(siligen_dispense_packaging_contracts_public INTERFACE cxx_std_17)
