if(NOT TARGET siligen_topology_feature_contour_augment_adapter)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/domain/geometry"
        "${CMAKE_BINARY_DIR}/modules/topology-feature/domain/geometry"
    )
endif()

if(NOT TARGET siligen_topology_feature_contour_augment_adapter)
    message(FATAL_ERROR "topology-feature requires canonical domain/geometry target: siligen_topology_feature_contour_augment_adapter")
endif()

add_library(siligen_module_topology_feature INTERFACE)
target_link_libraries(siligen_module_topology_feature INTERFACE
    siligen_topology_feature_contour_augment_adapter
)
target_compile_features(siligen_module_topology_feature INTERFACE cxx_std_17)

set_target_properties(siligen_module_topology_feature PROPERTIES
    SILIGEN_MODULE_ID "M3"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/topology-feature"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/topology-feature/domain;modules/topology-feature/services;modules/topology-feature/application;modules/topology-feature/adapters;modules/topology-feature/tests;modules/topology-feature/examples"
)
