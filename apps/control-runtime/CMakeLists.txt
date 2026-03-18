cmake_minimum_required(VERSION 3.20)

add_executable(siligen_control_runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

set_target_properties(siligen_control_runtime PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    OUTPUT_NAME "siligen_control_runtime"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(siligen_control_runtime PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${SILIGEN_RUNTIME_HOST_DIR}/src
)

target_link_libraries(siligen_control_runtime PRIVATE
    siligen_runtime_host
)

set(MULTICARD_DLL_PATH "${SILIGEN_MULTICARD_VENDOR_DIR}/MultiCard.dll")
if(EXISTS "${MULTICARD_DLL_PATH}")
    add_custom_command(TARGET siligen_control_runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MULTICARD_DLL_PATH}"
            "$<TARGET_FILE_DIR:siligen_control_runtime>"
        COMMENT "拷贝MultiCard.dll到 control-runtime 输出目录"
    )
endif()
