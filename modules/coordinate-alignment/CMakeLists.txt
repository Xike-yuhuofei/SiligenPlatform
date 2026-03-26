if(NOT TARGET siligen_coordinate_alignment_domain_machine)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/domain/machine"
        "${CMAKE_BINARY_DIR}/modules/coordinate-alignment/domain/machine"
    )
endif()

if(NOT TARGET siligen_coordinate_alignment_domain_machine)
    message(FATAL_ERROR "coordinate-alignment requires canonical domain/machine target: siligen_coordinate_alignment_domain_machine")
endif()

add_library(siligen_module_coordinate_alignment INTERFACE)
target_link_libraries(siligen_module_coordinate_alignment INTERFACE
    siligen_coordinate_alignment_domain_machine
)
target_compile_features(siligen_module_coordinate_alignment INTERFACE cxx_std_17)

set_target_properties(siligen_module_coordinate_alignment PROPERTIES
    SILIGEN_MODULE_ID "M5"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/coordinate-alignment"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/coordinate-alignment/domain;modules/coordinate-alignment/services;modules/coordinate-alignment/application;modules/coordinate-alignment/adapters;modules/coordinate-alignment/tests;modules/coordinate-alignment/examples"
)
