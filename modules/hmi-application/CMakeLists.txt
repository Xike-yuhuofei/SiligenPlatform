add_library(siligen_module_hmi_application INTERFACE)

add_custom_target(siligen_hmi_application_owner_assets
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/__init__.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/adapters/__init__.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/adapters/launch_supervision_env.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/adapters/launch_supervision_ports.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/adapters/logging_support.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/adapters/startup_qt_workers.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/contracts/__init__.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/contracts/launch_supervision_contract.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/domain/__init__.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/domain/launch_result_types.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/domain/launch_supervision_types.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/domain/preview_session_types.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/adapters/qt_workers.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/launch_supervision_session.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/launch_state.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/preview_gate.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/preview_session.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/services/__init__.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/services/launch_supervision_flow.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/services/launch_supervision_runtime.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/services/launch_result_projection.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/services/preview_playback.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/services/preview_payload_authority.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/services/preview_preflight.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/services/preview_status_projection.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/services/startup_sequence_service.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/services/startup_orchestration.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/startup.py"
)
add_dependencies(siligen_module_hmi_application siligen_hmi_application_owner_assets)

if(TARGET siligen_application_contracts)
    target_link_libraries(siligen_module_hmi_application INTERFACE siligen_application_contracts)
endif()

target_compile_features(siligen_module_hmi_application INTERFACE cxx_std_17)

set_target_properties(siligen_module_hmi_application PROPERTIES
    SILIGEN_MODULE_ID "M11"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/hmi-application"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/hmi-application/application;modules/hmi-application/tests"
)
