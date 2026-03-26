add_library(siligen_process_planning_domain_configuration STATIC
    CMPPulseConfig.cpp
    DispensingConfig.cpp
    InterpolationConfig.cpp
    MachineConfig.cpp
    ValveTimingConfig.cpp
)

target_include_directories(siligen_process_planning_domain_configuration PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)

target_link_libraries(siligen_process_planning_domain_configuration PUBLIC
    siligen_types
)

target_compile_features(siligen_process_planning_domain_configuration PUBLIC cxx_std_17)

message(STATUS "[ProcessPlanning::Domain::Configuration] canonical configuration target 已添加")
