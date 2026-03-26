add_library(siligen_module_dxf_geometry INTERFACE)

# M2 dxf-geometry owner entry anchor (Wave 2).
if (TARGET siligen_parsing_adapter)
    target_link_libraries(siligen_module_dxf_geometry INTERFACE siligen_parsing_adapter)
endif ()

target_compile_features(siligen_module_dxf_geometry INTERFACE cxx_std_17)

set_target_properties(siligen_module_dxf_geometry PROPERTIES
    SILIGEN_MODULE_ID "M2"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/dxf-geometry"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/dxf-geometry/domain;modules/dxf-geometry/services;modules/dxf-geometry/application;modules/dxf-geometry/adapters;modules/dxf-geometry/tests;modules/dxf-geometry/examples"
)
