add_library(siligen_module_hmi_application INTERFACE)

add_custom_target(siligen_hmi_application_owner_assets
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/__init__.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/launch_state.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/preview_gate.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/preview_session.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/startup.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/supervisor_contract.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/application/hmi_application/supervisor_session.py"
)
add_dependencies(siligen_module_hmi_application siligen_hmi_application_owner_assets)

if(TARGET siligen_application_contracts)
    target_link_libraries(siligen_module_hmi_application INTERFACE siligen_application_contracts)
endif()

target_compile_features(siligen_module_hmi_application INTERFACE cxx_std_17)

set_target_properties(siligen_module_hmi_application PROPERTIES
    SILIGEN_MODULE_ID "M11"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/hmi-application"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/hmi-application/domain;modules/hmi-application/services;modules/hmi-application/application;modules/hmi-application/adapters;modules/hmi-application/tests;modules/hmi-application/examples"
)
