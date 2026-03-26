set(SILIGEN_TRACE_DIAGNOSTICS_SOURCE_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/adapters"
)

if(NOT TARGET siligen_spdlog_adapter AND EXISTS "${SILIGEN_TRACE_DIAGNOSTICS_SOURCE_ROOT}/logging/spdlog/SpdlogLoggingAdapter.cpp")
    add_library(siligen_spdlog_adapter STATIC
        "${SILIGEN_TRACE_DIAGNOSTICS_SOURCE_ROOT}/logging/spdlog/SpdlogLoggingAdapter.cpp"
    )

    if(NOT TARGET siligen_traceability_spdlog_headers)
        add_library(siligen_traceability_spdlog_headers INTERFACE)
        target_include_directories(siligen_traceability_spdlog_headers INTERFACE
            ${SILIGEN_SPDLOG_INCLUDE_DIR}
        )
        target_compile_features(siligen_traceability_spdlog_headers INTERFACE cxx_std_17)
    endif()

    target_include_directories(siligen_spdlog_adapter PUBLIC
        "${SILIGEN_TRACE_DIAGNOSTICS_SOURCE_ROOT}"
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

if(NOT TARGET siligen_module_trace_diagnostics)
    add_library(siligen_module_trace_diagnostics INTERFACE)
endif()
target_link_libraries(siligen_module_trace_diagnostics INTERFACE siligen_spdlog_adapter)
target_compile_features(siligen_module_trace_diagnostics INTERFACE cxx_std_17)

set_target_properties(siligen_module_trace_diagnostics PROPERTIES
    SILIGEN_MODULE_ID "M10"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/trace-diagnostics"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/trace-diagnostics/adapters;modules/trace-diagnostics/contracts;modules/trace-diagnostics/tests;modules/trace-diagnostics/examples"
)
