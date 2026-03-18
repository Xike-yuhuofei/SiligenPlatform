# Parsing Adapter - PB 路径源适配器
# 职责: 加载 Python 预处理生成的 PB 路径数据

if(SILIGEN_ENABLE_PROTOBUF)
    set(SILIGEN_PROTO_ROOT "")
    set(SILIGEN_SHARED_PROTO_ROOT "${CMAKE_SOURCE_DIR}/../packages/engineering-contracts/proto/v1")

    if(EXISTS "${SILIGEN_SHARED_PROTO_ROOT}/dxf_primitives.proto")
        set(SILIGEN_PROTO_ROOT "${SILIGEN_SHARED_PROTO_ROOT}")
        message(STATUS "[Infrastructure] Using shared proto from engineering-contracts: ${SILIGEN_PROTO_ROOT}")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/proto/dxf_primitives.proto")
        set(SILIGEN_PROTO_ROOT "${CMAKE_SOURCE_DIR}/proto")
        message(WARNING "[Infrastructure] Falling back to local proto copy: ${SILIGEN_PROTO_ROOT}")
    else()
        message(FATAL_ERROR "[Infrastructure] dxf_primitives.proto not found in shared or local proto roots")
    endif()

    set(DXF_PROTO_FILE ${SILIGEN_PROTO_ROOT}/dxf_primitives.proto)
    set(DXF_PROTO_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto)
    file(MAKE_DIRECTORY ${DXF_PROTO_OUT_DIR})

    add_custom_command(
        OUTPUT
            ${DXF_PROTO_OUT_DIR}/dxf_primitives.pb.cc
            ${DXF_PROTO_OUT_DIR}/dxf_primitives.pb.h
        COMMAND $<TARGET_FILE:protobuf::protoc>
        ARGS --cpp_out=${DXF_PROTO_OUT_DIR} -I ${SILIGEN_PROTO_ROOT} ${DXF_PROTO_FILE}
        DEPENDS ${DXF_PROTO_FILE} protobuf::protoc
        COMMENT "Generating protobuf sources for DXF primitives"
    )

    set(DXF_PROTO_SRCS ${DXF_PROTO_OUT_DIR}/dxf_primitives.pb.cc)
endif()

add_library(siligen_parsing_adapter STATIC
    # Adapter 实现
    AutoPathSourceAdapter.cpp
    PbPathSourceAdapter.cpp
    DXFAdapterFactory.cpp
    DXFMigrationConfig.cpp
    DXFMigrationValidator.cpp
    DXFMigrationManager.cpp

    ${DXF_PROTO_SRCS}
)

target_include_directories(siligen_parsing_adapter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    ${CMAKE_SOURCE_DIR}/src
    ${DXF_PROTO_OUT_DIR}
)

target_link_libraries(siligen_parsing_adapter PUBLIC
    siligen_types
    siligen_domain
    protobuf::libprotobuf-lite
)

target_compile_features(siligen_parsing_adapter INTERFACE cxx_std_17)

message(STATUS "[Infrastructure] Parsing Adapter configured")
