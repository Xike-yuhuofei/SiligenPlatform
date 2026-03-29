if(NOT TARGET siligen_topology_feature_contracts_public AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/contracts/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts"
        "${CMAKE_BINARY_DIR}/modules/topology-feature/contracts"
    )
endif()

if(NOT TARGET siligen_topology_feature_contour_augment_adapter)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/adapters/infrastructure/adapters/planning/geometry"
        "${CMAKE_BINARY_DIR}/modules/topology-feature/adapters/infrastructure/adapters/planning/geometry"
    )
endif()

if(NOT TARGET siligen_topology_feature_contour_augment_adapter)
    message(FATAL_ERROR "topology-feature requires canonical adapter target: siligen_topology_feature_contour_augment_adapter")
endif()

add_library(siligen_module_topology_feature INTERFACE)
target_link_libraries(siligen_module_topology_feature INTERFACE
    siligen_topology_feature_contour_augment_adapter
)
if(TARGET siligen_topology_feature_contracts_public)
    target_link_libraries(siligen_module_topology_feature INTERFACE
        siligen_topology_feature_contracts_public
    )
endif()
target_compile_features(siligen_module_topology_feature INTERFACE cxx_std_17)

set_target_properties(siligen_module_topology_feature PROPERTIES
    SILIGEN_MODULE_ID "M3"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/topology-feature"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/topology-feature/domain;modules/topology-feature/services;modules/topology-feature/application;modules/topology-feature/adapters;modules/topology-feature/tests;modules/topology-feature/examples"
)
