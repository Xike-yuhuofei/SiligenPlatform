cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_trace_diagnostics_contracts_public AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../contracts/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../contracts"
        "${CMAKE_CURRENT_BINARY_DIR}/tdc"
    )
endif()

set(SILIGEN_TRACE_DIAGNOSTICS_ADAPTERS_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../adapters")
if(NOT TARGET siligen_spdlog_adapter AND EXISTS "${SILIGEN_TRACE_DIAGNOSTICS_ADAPTERS_SOURCE_ROOT}/logging/spdlog/SpdlogLoggingAdapter.cpp")
    add_library(siligen_spdlog_adapter STATIC
        "${SILIGEN_TRACE_DIAGNOSTICS_ADAPTERS_SOURCE_ROOT}/logging/spdlog/SpdlogLoggingAdapter.cpp"
    )

    if(NOT TARGET siligen_traceability_spdlog_headers)
        add_library(siligen_traceability_spdlog_headers INTERFACE)
        target_include_directories(siligen_traceability_spdlog_headers INTERFACE
            ${SILIGEN_SPDLOG_INCLUDE_DIR}
        )
        target_compile_features(siligen_traceability_spdlog_headers INTERFACE cxx_std_17)
    endif()

    target_include_directories(siligen_spdlog_adapter PUBLIC
        "${SILIGEN_TRACE_DIAGNOSTICS_ADAPTERS_SOURCE_ROOT}"
        ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
    )
    target_link_libraries(siligen_spdlog_adapter PUBLIC
        siligen_traceability_spdlog_headers
    )
    if(TARGET siligen_types)
        target_link_libraries(siligen_spdlog_adapter PRIVATE siligen_types)
    endif()
    target_compile_features(siligen_spdlog_adapter PUBLIC cxx_std_17)
endif()

set(SILIGEN_TRACE_DIAGNOSTICS_APPLICATION_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_TRACE_DIAGNOSTICS_APPLICATION_PUBLIC_INCLUDE_DIR}/application/services/trace_diagnostics/LoggingServiceFactory.h")
    message(FATAL_ERROR
        "trace-diagnostics canonical application header missing: "
        "${SILIGEN_TRACE_DIAGNOSTICS_APPLICATION_PUBLIC_INCLUDE_DIR}/application/services/trace_diagnostics/LoggingServiceFactory.h"
    )
endif()

if(NOT TARGET siligen_trace_diagnostics_application)
    add_library(siligen_trace_diagnostics_application STATIC
        services/trace_diagnostics/LoggingServiceFactory.cpp
    )
endif()
target_include_directories(siligen_trace_diagnostics_application PUBLIC
    ${SILIGEN_TRACE_DIAGNOSTICS_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(siligen_trace_diagnostics_application PUBLIC
    siligen_shared_kernel
    siligen_types
    siligen_spdlog_adapter
)
if(TARGET siligen_trace_diagnostics_contracts_public)
    target_link_libraries(siligen_trace_diagnostics_application PUBLIC
        siligen_trace_diagnostics_contracts_public
    )
endif()
target_compile_features(siligen_trace_diagnostics_application PUBLIC cxx_std_17)

if(NOT TARGET siligen_trace_diagnostics_application_public)
    add_library(siligen_trace_diagnostics_application_public INTERFACE)
endif()
target_link_libraries(siligen_trace_diagnostics_application_public INTERFACE
    $<LINK_ONLY:siligen_trace_diagnostics_application>
)
target_include_directories(siligen_trace_diagnostics_application_public INTERFACE
    ${SILIGEN_TRACE_DIAGNOSTICS_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
if(TARGET siligen_trace_diagnostics_contracts_public)
    target_link_libraries(siligen_trace_diagnostics_application_public INTERFACE
        siligen_trace_diagnostics_contracts_public
    )
endif()
target_compile_features(siligen_trace_diagnostics_application_public INTERFACE cxx_std_17)
