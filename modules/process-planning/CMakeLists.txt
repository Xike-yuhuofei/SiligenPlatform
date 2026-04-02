if(NOT TARGET siligen_process_planning_domain_configuration)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/domain/configuration"
        "${CMAKE_BINARY_DIR}/modules/process-planning/domain/configuration"
    )
endif()

if(NOT TARGET siligen_process_planning_domain_configuration)
    message(FATAL_ERROR "process-planning requires canonical domain/configuration target: siligen_process_planning_domain_configuration")
endif()

if(NOT TARGET siligen_process_planning_contracts_public AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/contracts/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts"
        "${CMAKE_BINARY_DIR}/modules/process-planning/contracts"
    )
endif()

if(NOT TARGET siligen_process_planning_application_public AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/application/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/application"
        "${CMAKE_BINARY_DIR}/modules/process-planning/application"
    )
endif()

if(NOT TARGET siligen_process_planning_application_public)
    message(FATAL_ERROR "process-planning requires canonical application target: siligen_process_planning_application_public")
endif()

add_library(siligen_module_process_planning INTERFACE)
target_link_libraries(siligen_module_process_planning INTERFACE
    siligen_process_planning_application_public
)
if(TARGET siligen_process_planning_contracts_public)
    target_link_libraries(siligen_module_process_planning INTERFACE
        siligen_process_planning_contracts_public
    )
endif()
target_compile_features(siligen_module_process_planning INTERFACE cxx_std_17)

set_target_properties(siligen_module_process_planning PROPERTIES
    SILIGEN_MODULE_ID "M4"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/process-planning"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/process-planning/domain;modules/process-planning/services;modules/process-planning/application;modules/process-planning/adapters;modules/process-planning/tests;modules/process-planning/examples"
)
