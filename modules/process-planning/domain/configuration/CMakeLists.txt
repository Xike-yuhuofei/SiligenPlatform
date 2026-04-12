if(NOT TARGET siligen_process_path_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../process-path/contracts"
        "${CMAKE_BINARY_DIR}/_p/ppdc_ppath"
    )
endif()

if(NOT TARGET siligen_motion_planning_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../motion-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/ppdc_mpc"
    )
endif()

add_library(siligen_process_planning_domain_configuration STATIC
    CMPPulseConfig.cpp
    DispensingConfig.cpp
    InterpolationConfig.cpp
    MachineConfig.cpp
    ValveTimingConfig.cpp
)

target_include_directories(siligen_process_planning_domain_configuration PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)

target_link_libraries(siligen_process_planning_domain_configuration PUBLIC
    siligen_types
    siligen_motion_planning_contracts_public
)

target_compile_features(siligen_process_planning_domain_configuration PUBLIC cxx_std_17)

message(STATUS "[ProcessPlanning::Domain::Configuration] canonical configuration target 已添加")
