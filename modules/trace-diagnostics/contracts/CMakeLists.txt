cmake_minimum_required(VERSION 3.20)

set(SILIGEN_TRACE_DIAGNOSTICS_CONTRACTS_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_TRACE_DIAGNOSTICS_CONTRACTS_PUBLIC_INCLUDE_DIR}/trace_diagnostics/contracts/TraceDiagnosticsContracts.h")
    message(FATAL_ERROR
        "trace-diagnostics canonical contracts header missing: "
        "${SILIGEN_TRACE_DIAGNOSTICS_CONTRACTS_PUBLIC_INCLUDE_DIR}/trace_diagnostics/contracts/TraceDiagnosticsContracts.h"
    )
endif()

if(NOT EXISTS "${SILIGEN_TRACE_DIAGNOSTICS_CONTRACTS_PUBLIC_INCLUDE_DIR}/trace_diagnostics/contracts/IDiagnosticsPort.h")
    message(FATAL_ERROR
        "trace-diagnostics diagnostics sink contract header missing: "
        "${SILIGEN_TRACE_DIAGNOSTICS_CONTRACTS_PUBLIC_INCLUDE_DIR}/trace_diagnostics/contracts/IDiagnosticsPort.h"
    )
endif()

if(NOT EXISTS "${SILIGEN_TRACE_DIAGNOSTICS_CONTRACTS_PUBLIC_INCLUDE_DIR}/trace_diagnostics/contracts/DiagnosticTypes.h")
    message(FATAL_ERROR
        "trace-diagnostics diagnostic value-object contract header missing: "
        "${SILIGEN_TRACE_DIAGNOSTICS_CONTRACTS_PUBLIC_INCLUDE_DIR}/trace_diagnostics/contracts/DiagnosticTypes.h"
    )
endif()

if(NOT EXISTS "${SILIGEN_TRACE_DIAGNOSTICS_CONTRACTS_PUBLIC_INCLUDE_DIR}/trace_diagnostics/contracts/ValidationEvidenceBundle.h")
    message(FATAL_ERROR
        "trace-diagnostics evidence contract header missing: "
        "${SILIGEN_TRACE_DIAGNOSTICS_CONTRACTS_PUBLIC_INCLUDE_DIR}/trace_diagnostics/contracts/ValidationEvidenceBundle.h"
    )
endif()

if(NOT TARGET siligen_trace_diagnostics_contracts_public)
    add_library(siligen_trace_diagnostics_contracts_public INTERFACE)
endif()
target_include_directories(siligen_trace_diagnostics_contracts_public INTERFACE
    ${SILIGEN_TRACE_DIAGNOSTICS_CONTRACTS_PUBLIC_INCLUDE_DIR}
)
target_link_libraries(siligen_trace_diagnostics_contracts_public INTERFACE
    siligen_shared_kernel
    siligen_types
)
if(TARGET siligen_application_contracts)
    target_link_libraries(siligen_trace_diagnostics_contracts_public INTERFACE
        siligen_application_contracts
    )
endif()
target_compile_features(siligen_trace_diagnostics_contracts_public INTERFACE cxx_std_17)
