if(NOT TARGET siligen_dispense_packaging_domain_dispensing)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/domain/dispensing"
        "${CMAKE_BINARY_DIR}/modules/dispense-packaging/domain/dispensing"
    )
endif()

if(NOT TARGET siligen_dispense_packaging_domain_dispensing)
    message(FATAL_ERROR "dispense-packaging requires canonical domain/dispensing target: siligen_dispense_packaging_domain_dispensing")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/contracts/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts"
        "${CMAKE_BINARY_DIR}/modules/dispense-packaging/contracts"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/application/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/application"
        "${CMAKE_BINARY_DIR}/modules/dispense-packaging/application"
    )
endif()

add_library(siligen_module_dispense_packaging INTERFACE)
if(NOT TARGET siligen_dispense_packaging_application_public)
    message(FATAL_ERROR
        "dispense-packaging canonical module target requires siligen_dispense_packaging_application_public; "
        "refusing to fall back to siligen_dispense_packaging_domain_dispensing"
    )
endif()
target_link_libraries(siligen_module_dispense_packaging INTERFACE
    siligen_dispense_packaging_application_public
)
if(TARGET siligen_dispense_packaging_contracts_public)
    target_link_libraries(siligen_module_dispense_packaging INTERFACE
        siligen_dispense_packaging_contracts_public
    )
endif()
target_compile_features(siligen_module_dispense_packaging INTERFACE cxx_std_17)

set_target_properties(siligen_module_dispense_packaging PROPERTIES
    SILIGEN_MODULE_ID "M8"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/dispense-packaging"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/dispense-packaging/domain;modules/dispense-packaging/services;modules/dispense-packaging/application;modules/dispense-packaging/adapters;modules/dispense-packaging/tests;modules/dispense-packaging/examples"
)
