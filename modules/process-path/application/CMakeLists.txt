cmake_minimum_required(VERSION 3.20)

set(SILIGEN_PROCESS_PATH_APPLICATION_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_PROCESS_PATH_APPLICATION_PUBLIC_INCLUDE_DIR}/application")
    message(FATAL_ERROR
        "process-path canonical application include root missing: "
        "${SILIGEN_PROCESS_PATH_APPLICATION_PUBLIC_INCLUDE_DIR}/application"
    )
endif()

if(NOT TARGET siligen_process_path_application_headers)
    add_library(siligen_process_path_application_headers INTERFACE)
endif()
target_include_directories(siligen_process_path_application_headers INTERFACE
    ${SILIGEN_PROCESS_PATH_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_process_path_application_headers INTERFACE
    siligen_process_path_contracts_public
)
if(TARGET siligen_coordinate_alignment_contracts_public)
    target_link_libraries(siligen_process_path_application_headers INTERFACE
        siligen_coordinate_alignment_contracts_public
    )
endif()
target_compile_features(siligen_process_path_application_headers INTERFACE cxx_std_17)

set(SILIGEN_PROCESS_PATH_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
    ${PROCESS_RUNTIME_CORE_IMPLEMENTATION_INCLUDE_DIRS}
)
list(REMOVE_DUPLICATES SILIGEN_PROCESS_PATH_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS)

add_library(siligen_process_path_application STATIC
    services/process_path/ProcessPathFacade.cpp
)
target_include_directories(siligen_process_path_application PUBLIC
    ${SILIGEN_PROCESS_PATH_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_process_path_application PRIVATE
    ${SILIGEN_PROCESS_PATH_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_process_path_application PUBLIC
    siligen_process_path_contracts_public
    siligen_types
    siligen_utils
)
target_link_libraries(siligen_process_path_application PRIVATE
    siligen_process_path_domain_trajectory
)
if(TARGET siligen_coordinate_alignment_contracts_public)
    target_link_libraries(siligen_process_path_application PUBLIC
        siligen_coordinate_alignment_contracts_public
    )
endif()
target_compile_features(siligen_process_path_application PUBLIC cxx_std_17)

if(NOT TARGET siligen_process_path_application_public)
    add_library(siligen_process_path_application_public INTERFACE)
endif()
target_link_libraries(siligen_process_path_application_public INTERFACE
    siligen_process_path_application_headers
    siligen_process_path_contracts_public
    $<LINK_ONLY:siligen_process_path_application>
)
target_compile_features(siligen_process_path_application_public INTERFACE cxx_std_17)
