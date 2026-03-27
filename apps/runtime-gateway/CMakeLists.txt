cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_process_planning_legacy_configuration_bridge_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/process-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/arg_ppc"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/transport-gateway/CMakeLists.txt" AND NOT TARGET siligen_transport_gateway)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/transport-gateway"
        "${CMAKE_BINARY_DIR}/apps/runtime-gateway/transport-gateway"
    )
endif()

if(NOT TARGET siligen_app_runtime_gateway)
    add_library(siligen_app_runtime_gateway INTERFACE)
endif()

target_link_libraries(siligen_app_runtime_gateway INTERFACE
    siligen_transport_gateway
    siligen_runtime_host
    siligen_job_ingest_contracts
    siligen_process_planning_legacy_configuration_bridge_public
)
target_compile_features(siligen_app_runtime_gateway INTERFACE cxx_std_17)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    add_executable(siligen_runtime_gateway
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    )
    set_target_properties(siligen_runtime_gateway PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        OUTPUT_NAME "siligen_runtime_gateway"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    target_include_directories(siligen_runtime_gateway PRIVATE
        ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
        ${SILIGEN_BOOST_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/modules/job-ingest/contracts/include
    )
    target_link_libraries(siligen_runtime_gateway PRIVATE siligen_app_runtime_gateway)
endif()
