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

if(NOT TARGET siligen_motion_planning_contracts_public AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/contracts/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts"
        "${CMAKE_BINARY_DIR}/modules/motion-planning/contracts"
    )
endif()

if(NOT TARGET siligen_motion_planning_application_public AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/application/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/application"
        "${CMAKE_BINARY_DIR}/modules/motion-planning/application"
    )
endif()

add_library(siligen_module_motion_planning INTERFACE)
if(NOT TARGET siligen_motion_planning_application_public)
    message(FATAL_ERROR "motion-planning requires canonical application public target")
endif()
target_link_libraries(siligen_module_motion_planning INTERFACE
    siligen_motion_planning_application_public
)
if(TARGET siligen_motion_planning_contracts_public)
    target_link_libraries(siligen_module_motion_planning INTERFACE
        siligen_motion_planning_contracts_public
    )
else()
    message(FATAL_ERROR "motion-planning requires canonical contracts public target")
endif()

target_compile_features(siligen_module_motion_planning INTERFACE cxx_std_17)

set_target_properties(siligen_module_motion_planning PROPERTIES
    SILIGEN_MODULE_ID "M7"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/motion-planning"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/motion-planning/domain;modules/motion-planning/application;modules/motion-planning/contracts;modules/motion-planning/tests"
)
