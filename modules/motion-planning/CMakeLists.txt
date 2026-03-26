if(NOT DEFINED SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR)
    set(SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

if(NOT TARGET siligen_motion)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/domain/motion"
        "${CMAKE_BINARY_DIR}/modules/motion-planning/domain/motion"
    )
endif()

if(NOT TARGET siligen_motion)
    message(FATAL_ERROR "motion-planning requires canonical domain/motion target: siligen_motion")
endif()

add_library(siligen_module_motion_planning INTERFACE)
target_link_libraries(siligen_module_motion_planning INTERFACE
    siligen_motion
)

target_compile_features(siligen_module_motion_planning INTERFACE cxx_std_17)

set_target_properties(siligen_module_motion_planning PROPERTIES
    SILIGEN_MODULE_ID "M7"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/motion-planning"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/motion-planning/domain;modules/motion-planning/services;modules/motion-planning/application;modules/motion-planning/adapters;modules/motion-planning/tests;modules/motion-planning/examples"
)
