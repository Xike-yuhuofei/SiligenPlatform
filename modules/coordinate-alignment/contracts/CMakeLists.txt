cmake_minimum_required(VERSION 3.20)

set(SILIGEN_COORDINATE_ALIGNMENT_CONTRACTS_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_COORDINATE_ALIGNMENT_CONTRACTS_PUBLIC_INCLUDE_DIR}/coordinate_alignment/contracts/CoordinateTransformSet.h")
    message(FATAL_ERROR
        "coordinate-alignment canonical contracts header missing: "
        "${SILIGEN_COORDINATE_ALIGNMENT_CONTRACTS_PUBLIC_INCLUDE_DIR}/coordinate_alignment/contracts/CoordinateTransformSet.h"
    )
endif()

if(NOT TARGET siligen_coordinate_alignment_contracts_public)
    add_library(siligen_coordinate_alignment_contracts_public INTERFACE)
endif()
target_include_directories(siligen_coordinate_alignment_contracts_public INTERFACE
    ${SILIGEN_COORDINATE_ALIGNMENT_CONTRACTS_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
if(TARGET siligen_application_contracts)
    target_link_libraries(siligen_coordinate_alignment_contracts_public INTERFACE
        siligen_application_contracts
    )
endif()
target_compile_features(siligen_coordinate_alignment_contracts_public INTERFACE cxx_std_17)
