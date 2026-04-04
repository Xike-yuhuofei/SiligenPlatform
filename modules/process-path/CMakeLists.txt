if(NOT TARGET siligen_process_path_domain_trajectory)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/domain/trajectory"
        "${CMAKE_BINARY_DIR}/modules/process-path/domain/trajectory"
    )
endif()

if(NOT TARGET siligen_process_path_domain_trajectory)
    message(FATAL_ERROR "process-path requires canonical domain/trajectory target: siligen_process_path_domain_trajectory")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/contracts/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts"
        "${CMAKE_BINARY_DIR}/modules/process-path/contracts"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/application/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/application"
        "${CMAKE_BINARY_DIR}/modules/process-path/application"
    )
endif()

add_library(siligen_module_process_path INTERFACE)
if(NOT TARGET siligen_process_path_application_public)
    message(FATAL_ERROR
        "process-path module target requires canonical public application target: "
        "siligen_process_path_application_public"
    )
endif()
if(NOT TARGET siligen_process_path_contracts_public)
    message(FATAL_ERROR
        "process-path module target requires canonical public contracts target: "
        "siligen_process_path_contracts_public"
    )
endif()
target_link_libraries(siligen_module_process_path INTERFACE
    siligen_process_path_application_public
    siligen_process_path_contracts_public
)
target_compile_features(siligen_module_process_path INTERFACE cxx_std_17)

set_target_properties(siligen_module_process_path PROPERTIES
    SILIGEN_MODULE_ID "M6"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/process-path"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/process-path/domain;modules/process-path/services;modules/process-path/application;modules/process-path/adapters;modules/process-path/tests;modules/process-path/examples"
)
