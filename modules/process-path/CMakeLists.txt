if(NOT TARGET siligen_process_path_domain_trajectory)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/domain/trajectory"
        "${CMAKE_BINARY_DIR}/modules/process-path/domain/trajectory"
    )
endif()

if(NOT TARGET siligen_process_path_domain_trajectory)
    message(FATAL_ERROR "process-path requires canonical domain/trajectory target: siligen_process_path_domain_trajectory")
endif()

add_library(siligen_module_process_path INTERFACE)
target_link_libraries(siligen_module_process_path INTERFACE
    siligen_process_path_domain_trajectory
)
target_compile_features(siligen_module_process_path INTERFACE cxx_std_17)

set_target_properties(siligen_module_process_path PROPERTIES
    SILIGEN_MODULE_ID "M6"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/process-path"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/process-path/domain;modules/process-path/services;modules/process-path/application;modules/process-path/adapters;modules/process-path/tests;modules/process-path/examples"
)
