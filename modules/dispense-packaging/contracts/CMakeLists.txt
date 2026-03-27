cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_dispense_packaging_contracts_public)
    add_library(siligen_dispense_packaging_contracts_public INTERFACE)
endif()

target_include_directories(siligen_dispense_packaging_contracts_public INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)

if(TARGET siligen_application_contracts)
    target_link_libraries(siligen_dispense_packaging_contracts_public INTERFACE
        siligen_application_contracts
    )
endif()

target_compile_features(siligen_dispense_packaging_contracts_public INTERFACE cxx_std_17)
