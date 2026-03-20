cmake_minimum_required(VERSION 3.20)

add_executable(siligen_tcp_server
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

set_target_properties(siligen_tcp_server PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    OUTPUT_NAME "siligen_tcp_server"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(siligen_tcp_server PRIVATE
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
    ${SILIGEN_BOOST_INCLUDE_DIR}
)

target_link_libraries(siligen_tcp_server PRIVATE
    siligen_transport_gateway
    siligen_runtime_host
)

set(MULTICARD_DLL_PATH "${SILIGEN_MULTICARD_VENDOR_DIR}/MultiCard.dll")
if(EXISTS "${MULTICARD_DLL_PATH}")
    add_custom_command(TARGET siligen_tcp_server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MULTICARD_DLL_PATH}"
            "$<TARGET_FILE_DIR:siligen_tcp_server>"
        COMMENT "拷贝MultiCard.dll到 TCP Server 输出目录"
    )
endif()
