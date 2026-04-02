cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_process_path_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../process-path/contracts"
        "${CMAKE_BINARY_DIR}/_p/dpa_ppc"
    )
endif()
if(NOT TARGET siligen_motion_planning_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../motion-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/dpa_mpc"
    )
endif()
if(NOT TARGET siligen_dispense_packaging_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../contracts"
        "${CMAKE_BINARY_DIR}/_p/dpa_dpc"
    )
endif()

set(SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}/application")
    message(FATAL_ERROR
        "dispense-packaging canonical application include root missing: "
        "${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}/application"
    )
endif()

if(NOT TARGET siligen_dispense_packaging_application_headers)
    add_library(siligen_dispense_packaging_application_headers INTERFACE)
endif()
target_include_directories(siligen_dispense_packaging_application_headers INTERFACE
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_dispense_packaging_application_headers INTERFACE
    siligen_dispense_packaging_domain_dispensing
)
if(TARGET siligen_dispense_packaging_contracts_public)
    target_link_libraries(siligen_dispense_packaging_application_headers INTERFACE
        siligen_dispense_packaging_contracts_public
    )
endif()
if(TARGET siligen_process_path_contracts_public)
    target_link_libraries(siligen_dispense_packaging_application_headers INTERFACE
        siligen_process_path_contracts_public
    )
endif()
if(TARGET siligen_motion_planning_contracts_public)
    target_link_libraries(siligen_dispense_packaging_application_headers INTERFACE
        siligen_motion_planning_contracts_public
    )
endif()
target_compile_features(siligen_dispense_packaging_application_headers INTERFACE cxx_std_17)

set(SILIGEN_DISPENSE_PACKAGING_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
    ${PROCESS_RUNTIME_CORE_IMPLEMENTATION_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
list(REMOVE_DUPLICATES SILIGEN_DISPENSE_PACKAGING_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS)

if(NOT TARGET siligen_dispense_packaging_application)
    add_library(siligen_dispense_packaging_application STATIC
        services/dispensing/PlanningAssemblyServices.cpp
        services/dispensing/PreviewSnapshotService.cpp
    )
endif()
target_include_directories(siligen_dispense_packaging_application PUBLIC
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_dispense_packaging_application PRIVATE
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_dispense_packaging_application PUBLIC
    siligen_dispense_packaging_domain_dispensing
    siligen_types
    siligen_utils
)
if(TARGET siligen_dispense_packaging_contracts_public)
    target_link_libraries(siligen_dispense_packaging_application PUBLIC
        siligen_dispense_packaging_contracts_public
    )
endif()
if(TARGET siligen_process_path_contracts_public)
    target_link_libraries(siligen_dispense_packaging_application PUBLIC
        siligen_process_path_contracts_public
    )
endif()
if(TARGET siligen_motion_planning_contracts_public)
    target_link_libraries(siligen_dispense_packaging_application PUBLIC
        siligen_motion_planning_contracts_public
    )
endif()
target_compile_features(siligen_dispense_packaging_application PUBLIC cxx_std_17)

if(NOT TARGET siligen_dispense_packaging_application_public)
    add_library(siligen_dispense_packaging_application_public INTERFACE)
endif()
target_link_libraries(siligen_dispense_packaging_application_public INTERFACE
    siligen_dispense_packaging_application_headers
    siligen_dispense_packaging_contracts_public
    $<LINK_ONLY:siligen_dispense_packaging_application>
)
target_compile_features(siligen_dispense_packaging_application_public INTERFACE cxx_std_17)
