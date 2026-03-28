if(NOT TARGET siligen_topology_feature_contour_augment_adapter)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../adapters/infrastructure/adapters/planning/geometry"
        "${CMAKE_CURRENT_BINARY_DIR}/compat_adapter"
    )
endif()
