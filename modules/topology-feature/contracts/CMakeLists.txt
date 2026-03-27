cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_topology_feature_contour_augment_adapter)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../domain/geometry"
        "${CMAKE_CURRENT_BINARY_DIR}/odg"
    )
endif()

set(SILIGEN_TOPOLOGY_FEATURE_CONTRACTS_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SILIGEN_TOPOLOGY_FEATURE_LEGACY_BRIDGE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/legacy-bridge/include")

if(NOT EXISTS "${SILIGEN_TOPOLOGY_FEATURE_CONTRACTS_PUBLIC_INCLUDE_DIR}/topology_feature/contracts")
    message(FATAL_ERROR
        "topology-feature canonical contracts include root missing: "
        "${SILIGEN_TOPOLOGY_FEATURE_CONTRACTS_PUBLIC_INCLUDE_DIR}/topology_feature/contracts"
    )
endif()

if(NOT EXISTS "${SILIGEN_TOPOLOGY_FEATURE_LEGACY_BRIDGE_INCLUDE_DIR}/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.h")
    message(FATAL_ERROR
        "topology-feature canonical legacy bridge include root missing: "
        "${SILIGEN_TOPOLOGY_FEATURE_LEGACY_BRIDGE_INCLUDE_DIR}/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.h"
    )
endif()

if(NOT TARGET siligen_topology_feature_contracts_public)
    add_library(siligen_topology_feature_contracts_public INTERFACE)
endif()
target_include_directories(siligen_topology_feature_contracts_public INTERFACE
    ${SILIGEN_TOPOLOGY_FEATURE_CONTRACTS_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_TOPOLOGY_FEATURE_LEGACY_BRIDGE_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_topology_feature_contracts_public INTERFACE
    siligen_topology_feature_contour_augment_adapter
)
target_compile_features(siligen_topology_feature_contracts_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_topology_feature_legacy_contour_bridge_public)
    add_library(siligen_topology_feature_legacy_contour_bridge_public INTERFACE)
endif()
target_include_directories(siligen_topology_feature_legacy_contour_bridge_public INTERFACE
    ${SILIGEN_TOPOLOGY_FEATURE_LEGACY_BRIDGE_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_topology_feature_legacy_contour_bridge_public INTERFACE
    siligen_topology_feature_contracts_public
    siligen_topology_feature_contour_augment_adapter
)
target_compile_features(siligen_topology_feature_legacy_contour_bridge_public INTERFACE cxx_std_17)
