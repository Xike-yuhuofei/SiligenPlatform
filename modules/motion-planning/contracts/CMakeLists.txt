cmake_minimum_required(VERSION 3.20)

set(SILIGEN_MOTION_PLANNING_CONTRACTS_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_MOTION_PLANNING_CONTRACTS_PUBLIC_INCLUDE_DIR}/motion_planning/contracts")
    message(FATAL_ERROR
        "motion-planning canonical contracts include root missing: "
        "${SILIGEN_MOTION_PLANNING_CONTRACTS_PUBLIC_INCLUDE_DIR}/motion_planning/contracts"
    )
endif()

if(NOT TARGET siligen_motion_planning_contracts_public)
    add_library(siligen_motion_planning_contracts_public INTERFACE)
endif()

target_include_directories(siligen_motion_planning_contracts_public INTERFACE
    ${SILIGEN_MOTION_PLANNING_CONTRACTS_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)

if(NOT TARGET siligen_process_path_contracts_public)
    message(FATAL_ERROR "motion-planning contracts require siligen_process_path_contracts_public")
endif()
target_link_libraries(siligen_motion_planning_contracts_public INTERFACE
    siligen_process_path_contracts_public
)

if(TARGET siligen_application_contracts)
    target_link_libraries(siligen_motion_planning_contracts_public INTERFACE
        siligen_application_contracts
    )
endif()

target_compile_features(siligen_motion_planning_contracts_public INTERFACE cxx_std_17)
