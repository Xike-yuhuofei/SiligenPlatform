cmake_minimum_required(VERSION 3.20)

set(SILIGEN_MOTION_PLANNING_APPLICATION_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_MOTION_PLANNING_APPLICATION_PUBLIC_INCLUDE_DIR}/application")
    message(FATAL_ERROR
        "motion-planning canonical application include root missing: "
        "${SILIGEN_MOTION_PLANNING_APPLICATION_PUBLIC_INCLUDE_DIR}/application"
    )
endif()

if(NOT TARGET siligen_motion_planning_application_headers)
    add_library(siligen_motion_planning_application_headers INTERFACE)
endif()
target_include_directories(siligen_motion_planning_application_headers INTERFACE
    ${SILIGEN_MOTION_PLANNING_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_motion_planning_application_headers INTERFACE
    siligen_motion_planning_contracts_public
    siligen_motion
)
target_compile_features(siligen_motion_planning_application_headers INTERFACE cxx_std_17)

set(SILIGEN_MOTION_PLANNING_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
    ${PROCESS_RUNTIME_CORE_IMPLEMENTATION_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
list(REMOVE_DUPLICATES SILIGEN_MOTION_PLANNING_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS)

add_library(siligen_motion_planning_application STATIC
    services/motion_planning/MotionPlanningFacade.cpp
)
target_include_directories(siligen_motion_planning_application PUBLIC
    ${SILIGEN_MOTION_PLANNING_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_motion_planning_application PRIVATE
    ${SILIGEN_MOTION_PLANNING_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_motion_planning_application PUBLIC
    siligen_motion_planning_contracts_public
    siligen_motion
    siligen_types
    siligen_utils
)
target_compile_features(siligen_motion_planning_application PUBLIC cxx_std_17)

if(NOT TARGET siligen_motion_planning_application_public)
    add_library(siligen_motion_planning_application_public INTERFACE)
endif()
target_link_libraries(siligen_motion_planning_application_public INTERFACE
    siligen_motion_planning_application_headers
    siligen_motion_planning_contracts_public
    $<LINK_ONLY:siligen_motion_planning_application>
)
target_compile_features(siligen_motion_planning_application_public INTERFACE cxx_std_17)
