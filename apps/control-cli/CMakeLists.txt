cmake_minimum_required(VERSION 3.20)

add_executable(siligen_cli
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

set_target_properties(siligen_cli PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    OUTPUT_NAME "siligen_cli"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(siligen_cli PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${SILIGEN_RUNTIME_HOST_DIR}/src
)

target_link_libraries(siligen_cli PRIVATE
    siligen_runtime_host
    siligen_control_application
    siligen_control_runtime_infrastructure
    siligen_domain
    siligen_shared
)

set(MULTICARD_DLL_PATH "${SILIGEN_MULTICARD_VENDOR_DIR}/MultiCard.dll")
if(EXISTS "${MULTICARD_DLL_PATH}")
    add_custom_command(TARGET siligen_cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MULTICARD_DLL_PATH}"
            "$<TARGET_FILE_DIR:siligen_cli>"
        COMMENT "拷贝MultiCard.dll到 CLI 输出目录"
    )
endif()
