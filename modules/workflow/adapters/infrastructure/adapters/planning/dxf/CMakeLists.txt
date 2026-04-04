# Parsing Adapter - PB 路径源适配器
# 职责: 加载 Python 预处理生成的 PB 路径数据

set(SILIGEN_DXF_ADAPTER_WORKSPACE_ROOT "${SILIGEN_WORKSPACE_ROOT}")
if(NOT SILIGEN_DXF_ADAPTER_WORKSPACE_ROOT)
    get_filename_component(
        SILIGEN_DXF_ADAPTER_WORKSPACE_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.."
        ABSOLUTE
    )
endif()

set(SILIGEN_DXF_ADAPTER_SHARED_INCLUDE_ROOT "${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}")
if(NOT SILIGEN_DXF_ADAPTER_SHARED_INCLUDE_ROOT)
    set(SILIGEN_DXF_ADAPTER_SHARED_INCLUDE_ROOT "${SILIGEN_DXF_ADAPTER_WORKSPACE_ROOT}/shared/kernel/include")
endif()

set(DXF_PROTO_SRCS)
set(DXF_PROTO_OUT_DIR)
set(DXF_PB_ADAPTER_LINK_LIBRARIES
    siligen_types
    siligen_utils
    siligen_process_path_contracts_public
)
set(DXF_LEGACY_COMPAT_LINK_LIBRARIES
    siligen_types
    siligen_utils
    siligen_process_path_contracts_public
)

set(SILIGEN_DXF_ADAPTER_PROTOBUF_ENABLED OFF)
if(SILIGEN_ENABLE_PROTOBUF AND TARGET protobuf::protoc AND TARGET protobuf::libprotobuf-lite)
    set(SILIGEN_DXF_ADAPTER_PROTOBUF_ENABLED ON)
endif()

if(SILIGEN_ENABLE_PROTOBUF AND NOT SILIGEN_DXF_ADAPTER_PROTOBUF_ENABLED)
    message(WARNING
        "[Infrastructure] protobuf targets are unavailable; siligen_parsing_adapter "
        "will fall back to SILIGEN_ENABLE_PROTOBUF=0 for this configure pass"
    )
endif()

if(SILIGEN_DXF_ADAPTER_PROTOBUF_ENABLED)
    set(SILIGEN_SHARED_PROTO_ROOT "${SILIGEN_DXF_ADAPTER_WORKSPACE_ROOT}/shared/contracts/engineering/proto/v1")
    if(NOT EXISTS "${SILIGEN_SHARED_PROTO_ROOT}/dxf_primitives.proto")
        message(FATAL_ERROR "[Infrastructure] dxf_primitives.proto not found in shared/contracts/engineering/proto/v1")
    endif()
    set(SILIGEN_PROTO_ROOT "${SILIGEN_SHARED_PROTO_ROOT}")
    message(STATUS "[Infrastructure] Using shared proto from engineering-contracts: ${SILIGEN_PROTO_ROOT}")

    set(DXF_PROTO_FILE ${SILIGEN_PROTO_ROOT}/dxf_primitives.proto)
    set(DXF_PROTO_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto)
    set(DXF_PROTO_RUNTIME_PATH "$<TARGET_FILE_DIR:protobuf::protoc>")
    if(TARGET abseil_dll)
        string(APPEND DXF_PROTO_RUNTIME_PATH ";$<TARGET_FILE_DIR:abseil_dll>")
    endif()
    if(TARGET utf8_validity)
        string(APPEND DXF_PROTO_RUNTIME_PATH ";$<TARGET_FILE_DIR:utf8_validity>")
    endif()
    string(APPEND DXF_PROTO_RUNTIME_PATH ";$ENV{PATH}")
    file(MAKE_DIRECTORY ${DXF_PROTO_OUT_DIR})

    add_custom_command(
        OUTPUT
            ${DXF_PROTO_OUT_DIR}/dxf_primitives.pb.cc
            ${DXF_PROTO_OUT_DIR}/dxf_primitives.pb.h
        COMMAND
            ${CMAKE_COMMAND} -E env
                "PATH=${DXF_PROTO_RUNTIME_PATH}"
                $<TARGET_FILE:protobuf::protoc>
                --cpp_out=${DXF_PROTO_OUT_DIR}
                -I ${SILIGEN_PROTO_ROOT}
                ${DXF_PROTO_FILE}
        DEPENDS ${DXF_PROTO_FILE} protobuf::protoc
        COMMENT "Generating protobuf sources for DXF primitives"
    )

    set(DXF_PROTO_SRCS ${DXF_PROTO_OUT_DIR}/dxf_primitives.pb.cc)
    list(APPEND DXF_PB_ADAPTER_LINK_LIBRARIES protobuf::libprotobuf-lite)
    list(APPEND DXF_LEGACY_COMPAT_LINK_LIBRARIES protobuf::libprotobuf-lite)
endif()

add_library(siligen_parsing_adapter STATIC
    PbPathSourceAdapter.cpp
    ${DXF_PROTO_SRCS}
)

option(
    SILIGEN_BUILD_WORKFLOW_DXF_LEGACY_COMPAT
    "Build workflow DXF legacy compat target for explicit legacy tests or migration validation."
    OFF
)

if(SILIGEN_BUILD_WORKFLOW_DXF_LEGACY_COMPAT)
    add_library(siligen_workflow_dxf_legacy_compat STATIC
        # Legacy DXF compat 实现
        AutoPathSourceAdapter.cpp
        DXFAdapterFactory.cpp
        DXFMigrationConfig.cpp
        DXFMigrationValidator.cpp
        DXFMigrationManager.cpp
    )
endif()

target_include_directories(siligen_parsing_adapter PUBLIC
    ${SILIGEN_WORKFLOW_ADAPTERS_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_include_directories(siligen_parsing_adapter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    ${SILIGEN_DXF_ADAPTER_SHARED_INCLUDE_ROOT}
    ${DXF_PROTO_OUT_DIR}
)
if(TARGET siligen_workflow_dxf_legacy_compat)
    target_include_directories(siligen_workflow_dxf_legacy_compat PUBLIC
        ${SILIGEN_WORKFLOW_ADAPTERS_PUBLIC_INCLUDE_DIR}
        ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}
        ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
    )
    target_include_directories(siligen_workflow_dxf_legacy_compat PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../..
        ${SILIGEN_DXF_ADAPTER_SHARED_INCLUDE_ROOT}
        ${DXF_PROTO_OUT_DIR}
    )
endif()

target_link_libraries(siligen_parsing_adapter PUBLIC ${DXF_PB_ADAPTER_LINK_LIBRARIES})
if(TARGET siligen_workflow_dxf_legacy_compat)
    target_link_libraries(siligen_workflow_dxf_legacy_compat PUBLIC
        ${DXF_LEGACY_COMPAT_LINK_LIBRARIES}
        siligen_parsing_adapter
    )
endif()

if(SILIGEN_DXF_ADAPTER_PROTOBUF_ENABLED)
    target_compile_definitions(siligen_parsing_adapter PRIVATE SILIGEN_ENABLE_PROTOBUF=1)
    if(TARGET siligen_workflow_dxf_legacy_compat)
        target_compile_definitions(siligen_workflow_dxf_legacy_compat PRIVATE SILIGEN_ENABLE_PROTOBUF=1)
    endif()
else()
    target_compile_definitions(siligen_parsing_adapter PRIVATE SILIGEN_ENABLE_PROTOBUF=0)
    if(TARGET siligen_workflow_dxf_legacy_compat)
        target_compile_definitions(siligen_workflow_dxf_legacy_compat PRIVATE SILIGEN_ENABLE_PROTOBUF=0)
    endif()
endif()

target_compile_features(siligen_parsing_adapter INTERFACE cxx_std_17)
if(TARGET siligen_workflow_dxf_legacy_compat)
    target_compile_features(siligen_workflow_dxf_legacy_compat INTERFACE cxx_std_17)
endif()

message(STATUS "[Infrastructure] Parsing Adapter configured")
