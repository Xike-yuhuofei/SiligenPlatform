# Contour augmentation adapter compatibility target

if(NOT TARGET siligen_topology_feature_legacy_contour_bridge_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../topology-feature/contracts"
        "${CMAKE_BINARY_DIR}/_p/wg_tfc"
    )
endif()

add_library(siligen_contour_augment_adapter INTERFACE)
target_include_directories(siligen_contour_augment_adapter INTERFACE
    ${SILIGEN_WORKSPACE_PUBLIC_INCLUDE_DIRS}
)
target_link_libraries(siligen_contour_augment_adapter INTERFACE
    siligen_topology_feature_legacy_contour_bridge_public
)
target_compile_features(siligen_contour_augment_adapter INTERFACE cxx_std_17)
