cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_process_planning_legacy_configuration_bridge_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/process-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/tg_ppc"
    )
endif()

add_library(siligen_transport_gateway_protocol STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/json_protocol.cpp
)

add_library(siligen_transport_gateway_boost_headers INTERFACE)
target_include_directories(siligen_transport_gateway_boost_headers INTERFACE
    ${SILIGEN_BOOST_INCLUDE_DIR}
)
target_compile_features(siligen_transport_gateway_boost_headers INTERFACE cxx_std_17)

add_library(siligen_transport_gateway_json_headers INTERFACE)
target_include_directories(siligen_transport_gateway_json_headers INTERFACE
    ${SILIGEN_NLOHMANN_INCLUDE_DIR}
)
target_compile_features(siligen_transport_gateway_json_headers INTERFACE cxx_std_17)

set_target_properties(siligen_transport_gateway_protocol PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

target_include_directories(siligen_transport_gateway_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_transport_gateway_protocol PUBLIC
    siligen_transport_gateway_json_headers
)

target_compile_features(siligen_transport_gateway_protocol PUBLIC cxx_std_17)

add_library(siligen_transport_gateway STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/facades/tcp/TcpDispensingFacade.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/facades/tcp/TcpMotionFacade.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/facades/tcp/TcpRecipeFacade.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/facades/tcp/TcpSystemFacade.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tcp/MockIoControlService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tcp/JsonProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tcp/TcpCommandDispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tcp/TcpServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tcp/TcpSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wiring/TcpServerHost.cpp
)

set_target_properties(siligen_transport_gateway PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

target_include_directories(siligen_transport_gateway PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)

target_link_libraries(siligen_transport_gateway PUBLIC
    siligen_transport_gateway_boost_headers
    siligen_transport_gateway_json_headers
    siligen_transport_gateway_protocol
    siligen_job_ingest_application_public
    siligen_workflow_application_public
    siligen_workflow_recipe_application_public
    siligen_workflow_recipe_serialization_public
    siligen_runtime_execution_application_public
    siligen_process_planning_legacy_configuration_bridge_public
    siligen_shared
)
target_link_libraries(siligen_transport_gateway PRIVATE
    siligen_runtime_process_bootstrap_public
)

target_compile_definitions(siligen_transport_gateway PUBLIC
    BOOST_ASIO_STANDALONE=0
    BOOST_ASIO_NO_DEPRECATED
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0601
)

if(WIN32)
    target_link_libraries(siligen_transport_gateway PUBLIC
        kernel32
        user32
        ws2_32
        mswsock
    )
endif()

target_compile_features(siligen_transport_gateway PUBLIC cxx_std_17)
set_target_properties(siligen_transport_gateway PROPERTIES
    SILIGEN_MIGRATION_SOURCE_ROOT "legacy-archive/transport-gateway"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "apps/runtime-gateway"
)
