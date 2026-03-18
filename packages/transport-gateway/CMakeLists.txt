cmake_minimum_required(VERSION 3.20)

add_library(siligen_transport_gateway_protocol STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/json_protocol.cpp
)

set_target_properties(siligen_transport_gateway_protocol PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

target_include_directories(siligen_transport_gateway_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
)

target_compile_features(siligen_transport_gateway_protocol PUBLIC cxx_std_17)

add_library(siligen_transport_gateway STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/facades/tcp/TcpDispensingFacade.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/facades/tcp/TcpMotionFacade.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/facades/tcp/TcpRecipeFacade.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/facades/tcp/TcpSystemFacade.cpp
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
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/third_party/boost
)

target_link_libraries(siligen_transport_gateway PUBLIC
    siligen_transport_gateway_protocol
    siligen_control_application
    siligen_control_runtime_infrastructure
    siligen_domain
    siligen_shared
    storage_adapters
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
