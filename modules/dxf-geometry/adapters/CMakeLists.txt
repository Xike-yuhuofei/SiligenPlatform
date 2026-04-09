cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_dxf_geometry_pb_path_source_adapter)
    set(SILIGEN_DXF_GEOMETRY_ADAPTERS_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
    foreach(required_dir
        IN ITEMS
            "${SILIGEN_DXF_GEOMETRY_ADAPTERS_PUBLIC_INCLUDE_DIR}/dxf_geometry"
            "${CMAKE_CURRENT_SOURCE_DIR}/dxf"
    )
        if(NOT EXISTS "${required_dir}")
            message(FATAL_ERROR "dxf-geometry canonical adapters root missing: ${required_dir}")
        endif()
    endforeach()

    set(SILIGEN_DXF_GEOMETRY_ADAPTER_WORKSPACE_ROOT "${SILIGEN_WORKSPACE_ROOT}")
    if(NOT SILIGEN_DXF_GEOMETRY_ADAPTER_WORKSPACE_ROOT)
        get_filename_component(
            SILIGEN_DXF_GEOMETRY_ADAPTER_WORKSPACE_ROOT
            "${CMAKE_CURRENT_SOURCE_DIR}/../../.."
            ABSOLUTE
        )
    endif()

    set(DXF_PROTO_SRCS)
    set(DXF_PROTO_OUT_DIR)
    set(DXF_PB_ADAPTER_LINK_LIBRARIES
        siligen_types
        siligen_utils
        siligen_process_path_contracts_public
    )

    set(SILIGEN_DXF_GEOMETRY_ADAPTER_PROTOBUF_ENABLED OFF)
    if(SILIGEN_ENABLE_PROTOBUF AND TARGET protobuf::protoc AND TARGET protobuf::libprotobuf-lite)
        set(SILIGEN_DXF_GEOMETRY_ADAPTER_PROTOBUF_ENABLED ON)
    endif()

    if(SILIGEN_ENABLE_PROTOBUF AND NOT SILIGEN_DXF_GEOMETRY_ADAPTER_PROTOBUF_ENABLED)
        message(WARNING
            "[DXFGeometry] protobuf targets are unavailable; siligen_dxf_geometry_pb_path_source_adapter "
            "will fall back to SILIGEN_ENABLE_PROTOBUF=0 for this configure pass"
        )
    endif()

    if(SILIGEN_DXF_GEOMETRY_ADAPTER_PROTOBUF_ENABLED)
        set(SILIGEN_SHARED_PROTO_ROOT
            "${SILIGEN_DXF_GEOMETRY_ADAPTER_WORKSPACE_ROOT}/shared/contracts/engineering/proto/v1"
        )
        if(NOT EXISTS "${SILIGEN_SHARED_PROTO_ROOT}/dxf_primitives.proto")
            message(FATAL_ERROR
                "[DXFGeometry] dxf_primitives.proto not found in shared/contracts/engineering/proto/v1"
            )
        endif()
        set(SILIGEN_PROTO_ROOT "${SILIGEN_SHARED_PROTO_ROOT}")

        set(DXF_PROTO_FILE "${SILIGEN_PROTO_ROOT}/dxf_primitives.proto")
        set(DXF_PROTO_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/dxf/proto")
        set(DXF_PROTO_RUNTIME_PATH "$<TARGET_FILE_DIR:protobuf::protoc>")
        if(TARGET abseil_dll)
            string(APPEND DXF_PROTO_RUNTIME_PATH ";$<TARGET_FILE_DIR:abseil_dll>")
        endif()
        if(TARGET utf8_validity)
            string(APPEND DXF_PROTO_RUNTIME_PATH ";$<TARGET_FILE_DIR:utf8_validity>")
        endif()
        string(APPEND DXF_PROTO_RUNTIME_PATH ";$ENV{PATH}")
        file(MAKE_DIRECTORY "${DXF_PROTO_OUT_DIR}")

        add_custom_command(
            OUTPUT
                "${DXF_PROTO_OUT_DIR}/dxf_primitives.pb.cc"
                "${DXF_PROTO_OUT_DIR}/dxf_primitives.pb.h"
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

        set(DXF_PROTO_SRCS "${DXF_PROTO_OUT_DIR}/dxf_primitives.pb.cc")
        list(APPEND DXF_PB_ADAPTER_LINK_LIBRARIES protobuf::libprotobuf-lite)
    endif()

    add_library(siligen_dxf_geometry_pb_path_source_adapter STATIC
        dxf/PbPathSourceAdapter.cpp
        ${DXF_PROTO_SRCS}
    )

    target_include_directories(siligen_dxf_geometry_pb_path_source_adapter PUBLIC
        ${SILIGEN_DXF_GEOMETRY_ADAPTERS_PUBLIC_INCLUDE_DIR}
        ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
    )
    target_include_directories(siligen_dxf_geometry_pb_path_source_adapter PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DXF_PROTO_OUT_DIR}
    )

    target_link_libraries(siligen_dxf_geometry_pb_path_source_adapter PUBLIC ${DXF_PB_ADAPTER_LINK_LIBRARIES})

    if(SILIGEN_DXF_GEOMETRY_ADAPTER_PROTOBUF_ENABLED)
        target_compile_definitions(siligen_dxf_geometry_pb_path_source_adapter PRIVATE SILIGEN_ENABLE_PROTOBUF=1)
    else()
        target_compile_definitions(siligen_dxf_geometry_pb_path_source_adapter PRIVATE SILIGEN_ENABLE_PROTOBUF=0)
    endif()

    target_compile_features(siligen_dxf_geometry_pb_path_source_adapter PUBLIC cxx_std_17)

    message(STATUS "[DXFGeometry] PbPathSource adapter configured")
endif()
