cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_coordinate_alignment_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../contracts"
        "${CMAKE_CURRENT_BINARY_DIR}/cac"
    )
endif()

if(NOT TARGET siligen_coordinate_alignment_domain_machine)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../domain/machine"
        "${CMAKE_CURRENT_BINARY_DIR}/odm"
    )
endif()

set(SILIGEN_COORDINATE_ALIGNMENT_APPLICATION_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_COORDINATE_ALIGNMENT_APPLICATION_PUBLIC_INCLUDE_DIR}/coordinate_alignment/application")
    message(FATAL_ERROR
        "coordinate-alignment canonical application include root missing: "
        "${SILIGEN_COORDINATE_ALIGNMENT_APPLICATION_PUBLIC_INCLUDE_DIR}/coordinate_alignment/application"
    )
endif()

if(NOT TARGET siligen_coordinate_alignment_application_headers)
    add_library(siligen_coordinate_alignment_application_headers INTERFACE)
endif()
target_include_directories(siligen_coordinate_alignment_application_headers INTERFACE
    ${SILIGEN_COORDINATE_ALIGNMENT_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_coordinate_alignment_application_headers INTERFACE
    siligen_coordinate_alignment_contracts_public
    siligen_coordinate_alignment_domain_machine
)
target_compile_features(siligen_coordinate_alignment_application_headers INTERFACE cxx_std_17)

if(NOT TARGET siligen_coordinate_alignment_application_public)
    add_library(siligen_coordinate_alignment_application_public INTERFACE)
endif()
target_link_libraries(siligen_coordinate_alignment_application_public INTERFACE
    siligen_coordinate_alignment_application_headers
    siligen_coordinate_alignment_contracts_public
    siligen_coordinate_alignment_domain_machine
)
target_compile_features(siligen_coordinate_alignment_application_public INTERFACE cxx_std_17)
