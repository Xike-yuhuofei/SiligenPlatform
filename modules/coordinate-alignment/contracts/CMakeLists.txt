cmake_minimum_required(VERSION 3.20)

set(SILIGEN_COORDINATE_ALIGNMENT_CONTRACTS_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_COORDINATE_ALIGNMENT_CONTRACTS_PUBLIC_INCLUDE_DIR}/coordinate_alignment/contracts/CoordinateTransformSet.h")
    message(FATAL_ERROR
        "coordinate-alignment canonical contracts header missing: "
        "${SILIGEN_COORDINATE_ALIGNMENT_CONTRACTS_PUBLIC_INCLUDE_DIR}/coordinate_alignment/contracts/CoordinateTransformSet.h"
    )
endif()

get_filename_component(SILIGEN_COORDINATE_ALIGNMENT_WORKSPACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

if(NOT TARGET siligen_types AND EXISTS "${SILIGEN_COORDINATE_ALIGNMENT_WORKSPACE_ROOT}/shared/kernel/CMakeLists.txt")
    add_subdirectory(
        "${SILIGEN_COORDINATE_ALIGNMENT_WORKSPACE_ROOT}/shared/kernel"
        "${CMAKE_BINARY_DIR}/shared/kernel"
    )
endif()

if(NOT TARGET siligen_coordinate_alignment_contracts_public)
    add_library(siligen_coordinate_alignment_contracts_public INTERFACE)
endif()
target_include_directories(siligen_coordinate_alignment_contracts_public INTERFACE
    ${SILIGEN_COORDINATE_ALIGNMENT_CONTRACTS_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_coordinate_alignment_contracts_public INTERFACE
    siligen_types
)
if(TARGET siligen_application_contracts)
    target_link_libraries(siligen_coordinate_alignment_contracts_public INTERFACE
        siligen_application_contracts
    )
endif()
target_compile_features(siligen_coordinate_alignment_contracts_public INTERFACE cxx_std_17)
