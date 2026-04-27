cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_process_planning_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../contracts"
        "${CMAKE_CURRENT_BINARY_DIR}/ppc"
    )
endif()
if(NOT TARGET siligen_process_planning_domain_configuration)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../domain/configuration"
        "${CMAKE_CURRENT_BINARY_DIR}/ppdc"
    )
endif()

set(SILIGEN_PROCESS_PLANNING_APPLICATION_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_PROCESS_PLANNING_APPLICATION_PUBLIC_INCLUDE_DIR}/process_planning/application")
    message(FATAL_ERROR
        "process-planning canonical application include root missing: "
        "${SILIGEN_PROCESS_PLANNING_APPLICATION_PUBLIC_INCLUDE_DIR}/process_planning/application"
    )
endif()

if(NOT TARGET siligen_process_planning_application_headers)
    add_library(siligen_process_planning_application_headers INTERFACE)
endif()
target_include_directories(siligen_process_planning_application_headers INTERFACE
    ${SILIGEN_PROCESS_PLANNING_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_link_libraries(siligen_process_planning_application_headers INTERFACE
    siligen_process_planning_contracts_public
)
target_compile_features(siligen_process_planning_application_headers INTERFACE cxx_std_17)

if(NOT TARGET siligen_process_planning_application_public)
    add_library(siligen_process_planning_application_public INTERFACE)
endif()
target_link_libraries(siligen_process_planning_application_public INTERFACE
    siligen_process_planning_application_headers
    siligen_process_planning_contracts_public
    $<LINK_ONLY:siligen_process_planning_domain_configuration>
)
target_compile_features(siligen_process_planning_application_public INTERFACE cxx_std_17)
